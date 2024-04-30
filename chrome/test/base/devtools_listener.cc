// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/devtools_listener.h"

#include <stddef.h>

#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/hash/md5.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "url/url_util.h"

namespace coverage {

namespace {

std::string_view SpanToStringPiece(const base::span<const uint8_t>& s) {
  return {reinterpret_cast<const char*>(s.data()), s.size()};
}

std::string EncodeURIComponent(const std::string& component) {
  url::RawCanonOutputT<char> encoded;
  url::EncodeURIComponent(component, &encoded);
  return std::string(encoded.view());
}

}  // namespace

DevToolsListener::DevToolsListener(content::DevToolsAgentHost* host,
                                   uint32_t uuid)
    : uuid_(base::StringPrintf("%u", uuid)) {
  CHECK(!host->IsAttached());
  host->AttachClient(this);
  Start(host);
}

DevToolsListener::~DevToolsListener() = default;

void DevToolsListener::Navigated(content::DevToolsAgentHost* host) {
  CHECK(host->IsAttached() && attached_);
  navigated_ = StartJSCoverage(host);
}

bool DevToolsListener::HasCoverage(content::DevToolsAgentHost* host) {
  return attached_ && navigated_;
}

void DevToolsListener::GetCoverage(content::DevToolsAgentHost* host,
                                   const base::FilePath& store,
                                   const std::string& test) {
  if (HasCoverage(host))
    StopAndStoreJSCoverage(host, store, test);
  navigated_ = false;
}

void DevToolsListener::Detach(content::DevToolsAgentHost* host) {
  if (attached_)
    host->DetachClient(this);
  navigated_ = false;
  attached_ = false;
}

std::string DevToolsListener::HostString(content::DevToolsAgentHost* host,
                                         const std::string& prefix) {
  std::string result = base::StrCat(
      {prefix, " ", host->GetType(), " title: ", host->GetTitle()});
  std::string description = host->GetDescription();
  if (!description.empty())
    base::StrAppend(&result, {" description: ", description});
  std::string url = host->GetURL().spec();
  if (!url.empty())
    base::StrAppend(&result, {" URL: ", url});
  return result;
}

void DevToolsListener::SetupCoverageStore(const base::FilePath& store_path) {
  if (!base::PathExists(store_path))
    CHECK(base::CreateDirectory(store_path));

  base::FilePath tests = store_path.AppendASCII("tests");
  if (!base::PathExists(tests))
    CHECK(base::CreateDirectory(tests));

  base::FilePath scripts = store_path.AppendASCII("scripts");
  if (!base::PathExists(scripts))
    CHECK(base::CreateDirectory(scripts));
}

void DevToolsListener::Start(content::DevToolsAgentHost* host) {
  std::string enable_runtime = "{\"id\":10,\"method\":\"Runtime.enable\"}";
  SendCommandMessage(host, enable_runtime);

  std::string enable_page = "{\"id\":11,\"method\":\"Page.enable\"}";
  SendCommandMessage(host, enable_page);
}

bool DevToolsListener::StartJSCoverage(content::DevToolsAgentHost* host) {
  std::string enable_profiler = "{\"id\":20,\"method\":\"Profiler.enable\"}";
  SendCommandMessage(host, enable_profiler);

  std::string start_precise_coverage =
      "{\"id\":21,\"method\":\"Profiler.startPreciseCoverage\",\"params\":{"
      "\"callCount\":true,\"detailed\":true}}";
  SendCommandMessage(host, start_precise_coverage);

  std::string enable_debugger = "{\"id\":22,\"method\":\"Debugger.enable\"}";
  SendCommandMessage(host, enable_debugger);

  std::string skip_all_pauses =
      "{\"id\":23,\"method\":\"Debugger.setSkipAllPauses\""
      ",\"params\":{\"skip\":true}}";
  SendCommandMessage(host, skip_all_pauses);

  return true;
}

void DevToolsListener::StopAndStoreJSCoverage(content::DevToolsAgentHost* host,
                                              const base::FilePath& store,
                                              const std::string& test) {
  std::string get_precise_coverage =
      "{\"id\":40,\"method\":\"Profiler.takePreciseCoverage\"}";
  SendCommandMessage(host, get_precise_coverage);
  if (!AwaitCommandResponse(40)) {
    LOG(ERROR) << "Host has been destroyed whilst getting precise coverage";
    return;
  }

  script_coverage_ = std::move(value_);
  base::Value::Dict* result = script_coverage_.FindDict("result");
  CHECK(result) << "result key is null: " << script_coverage_;

  base::Value::List* coverage_entries = result->FindList("result");
  CHECK(coverage_entries) << "Can't find result key: " << *result;

  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  VerifyAllScriptsAreParsedRepeatedly(coverage_entries, run_loop.QuitClosure(),
                                      /*retries=*/10);
  run_loop.Run();
  CHECK(all_scripts_parsed_) << "All scripts in coverage results were not "
                                "retrieved after 10s of waiting";

  StoreScripts(host, store);

  std::string stop_debugger = "{\"id\":41,\"method\":\"Debugger.disable\"}";
  SendCommandMessage(host, stop_debugger);

  std::string stop_profiler = "{\"id\":42,\"method\":\"Profiler.disable\"}";
  SendCommandMessage(host, stop_profiler);

  base::Value::List entries;
  for (base::Value& entry_value : *coverage_entries) {
    CHECK(entry_value.is_dict()) << "Entry is not dictionary: " << entry_value;
    base::Value::Dict& entry = entry_value.GetDict();
    std::string* script_id = entry.FindString("scriptId");
    CHECK(script_id) << "Can't find scriptId: " << entry;
    const auto it = script_id_map_.find(*script_id);
    if (it == script_id_map_.end())
      continue;

    entry.Set("hash", it->second);
    entries.Append(entry.Clone());
  }

  std::string url = host->GetURL().spec();
  result->Set("encodedHostURL", EncodeURIComponent(url));
  result->Set("hostTitle", host->GetTitle());
  result->Set("hostType", host->GetType());
  result->Set("hostTest", test);
  result->Set("hostURL", url);

  std::string md5 = base::MD5String(HostString(host, test));
  std::string coverage = base::StrCat({test, ".", md5, uuid_, ".cov.json"});
  base::FilePath path = store.AppendASCII("tests").AppendASCII(coverage);

  result->Set("result", std::move(entries));
  CHECK(base::JSONWriter::Write(*result, &coverage));
  base::WriteFile(path, coverage);

  script_coverage_.clear();
  script_hash_map_.clear();
  script_id_map_.clear();
  scripts_.clear();

  LOG_IF(ERROR, !AwaitCommandResponse(42))
      << "Host has been destroyed whilst waiting, coverage coverage already "
         "extracted though";
  value_.clear();
  all_scripts_parsed_ = false;
}

void DevToolsListener::VerifyAllScriptsAreParsedRepeatedly(
    const base::Value::List* coverage_entries,
    base::OnceClosure done_callback,
    int retries) {
  CHECK_GT(retries, 0);
  CHECK(done_callback);

  // Collect all the scriptId's that have been seen via the aggregated
  // `Debugger.scriptParsed` events.
  std::set<std::string> script_ids;
  for (base::Value::Dict& script : scripts_) {
    std::string* id = script.FindStringByDottedPath("params.scriptId");
    if (!id) {
      continue;
    }
    script_ids.emplace(*id);
  }

  // All the scriptId values seen in the coverage values must have been sent via
  // the `Debugger.scriptParsed` event. This tries 10 times with a 1 second
  // pause in between verification attempts.
  bool missing_script = false;
  for (const auto& entry : *coverage_entries) {
    const std::string* id = entry.GetDict().FindString("scriptId");
    CHECK(id) << "Can't extract scriptId: " << entry;
    if (!script_ids.contains(*id)) {
      missing_script = true;
      break;
    }
  }

  all_scripts_parsed_ = !missing_script;
  if (all_scripts_parsed_ || --retries == 0) {
    std::move(done_callback).Run();
    return;
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&DevToolsListener::VerifyAllScriptsAreParsedRepeatedly,
                     weak_ptr_factory_.GetWeakPtr(), coverage_entries,
                     std::move(done_callback), retries),
      base::Seconds(1));
}

void DevToolsListener::StoreScripts(content::DevToolsAgentHost* host,
                                    const base::FilePath& store) {
  for (base::Value::Dict& script : scripts_) {
    std::string id;
    {
      std::string* id_ptr = script.FindStringByDottedPath("params.scriptId");
      CHECK(id_ptr);
      CHECK(!id_ptr->empty());
      id = *id_ptr;
    }

    std::string url;
    {
      std::string* url_ptr = script.FindStringByDottedPath("params.url");
      if (!url_ptr)
        url_ptr = script.FindStringByDottedPath("params.sourceURL");
      if (!url_ptr || url_ptr->empty()) {
        value_.clear();
        continue;
      }
      url = *url_ptr;
    }

    std::string get_script_source = base::StringPrintf(
        "{\"id\":50,\"method\":\"Debugger.getScriptSource\""
        ",\"params\":{\"scriptId\":\"%s\"}}",
        id.c_str());
    SendCommandMessage(host, get_script_source);
    if (!AwaitCommandResponse(50)) {
      LOG(ERROR) << "Host has been destroyed whilst getting script source, "
                    "skipping remaining script sources";
      return;
    }

    std::string text;
    {
      base::Value::Dict* result = value_.FindDict("result");
      // TODO(crbug.com/40180762): In some cases the v8 isolate may clear out
      // the script source during execution. This can lead to the Debugger
      // seeing a scriptId during execution but when it comes time to retrieving
      // the source can no longer find the ID. For now we simply ignore these,
      // but we need to find a better way to handle this.
      if (!result) {
        LOG(ERROR) << "Can't find result from Debugger.getScriptSource: "
                   << value_;
        return;
      }
      std::string* text_ptr = result->FindString("scriptSource");
      if (!text_ptr || text_ptr->empty()) {
        value_.clear();
        continue;
      }
      text = *text_ptr;
    }

    std::string hash;
    {
      std::string* hash_ptr = script.FindStringByDottedPath("params.hash");
      CHECK(hash_ptr);
      hash = *hash_ptr;
    }

    if (script_id_map_.find(id) != script_id_map_.end())
      LOG(FATAL) << "Duplicate script by id " << url;
    script_id_map_[id] = hash;
    CHECK(!hash.empty());
    if (script_hash_map_.find(hash) != script_hash_map_.end()) {
      value_.clear();
      continue;
    }
    script_hash_map_[hash] = id;

    base::Value::Dict* params = script.FindDict("params");
    CHECK(params) << "Can't find params from script: " << script;

    params->Set("encodedURL", EncodeURIComponent(url));
    params->Set("hash", hash);
    params->Set("text", text);
    params->Set("url", url);

    base::FilePath path =
        store.AppendASCII("scripts").AppendASCII(hash.append(".js.json"));
    CHECK(base::JSONWriter::Write(*params, &text));
    if (!base::PathExists(path))  // script de-duplication
      base::WriteFile(path, text);
    value_.clear();
  }
}

void DevToolsListener::SendCommandMessage(content::DevToolsAgentHost* host,
                                          const std::string& command) {
  auto message = base::as_bytes(base::make_span(command));
  host->DispatchProtocolMessage(this, message);
}

bool DevToolsListener::AwaitCommandResponse(int id) {
  if (!attached_ && !navigated_) {
    return false;
  }
  value_.clear();
  value_id_ = id;

  base::RunLoop run_loop;
  value_closure_ = run_loop.QuitClosure();
  run_loop.Run();
  return attached_ && navigated_;
}

void DevToolsListener::DispatchProtocolMessage(
    content::DevToolsAgentHost* host,
    base::span<const uint8_t> message) {
  if (!navigated_)
    return;

  if (VLOG_IS_ON(2))
    VLOG(2) << SpanToStringPiece(message);

  std::optional<base::Value> value =
      base::JSONReader::Read(SpanToStringPiece(message));
  CHECK(value.has_value()) << "Cannot parse as JSON: "
                           << SpanToStringPiece(message);

  base::Value::Dict dict_value = std::move(value.value().GetDict());
  std::string* method = dict_value.FindString("method");
  if (method) {
    if (*method == "Runtime.executionContextsCreated") {
      scripts_.clear();
    } else if (*method == "Debugger.scriptParsed" && !all_scripts_parsed_) {
      scripts_.push_back(std::move(dict_value));
    }
    return;
  }

  std::optional<int> id = dict_value.FindInt("id");
  if (id.has_value() && id.value() == value_id_) {
    value_ = std::move(dict_value);
    CHECK(value_closure_);
    std::move(value_closure_).Run();
  }
}

bool DevToolsListener::MayAttachToURL(const GURL& url, bool is_webui) {
  return true;
}

void DevToolsListener::AgentHostClosed(content::DevToolsAgentHost* host) {
  navigated_ = false;
  attached_ = false;
  if (value_closure_) {
    std::move(value_closure_).Run();
  }
}

}  // namespace coverage
