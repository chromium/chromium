// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/memory_internals_ui.h"

#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/process/process_handle.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiling_host/profiling_process_host.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/memory_internals_resources.h"
#include "chrome/grit/memory_internals_resources_map.h"
#include "components/heap_profiling/multi_process/supervisor.h"
#include "components/services/heap_profiling/public/cpp/settings.h"
#include "components/services/heap_profiling/public/mojom/heap_profiling_service.mojom.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/common/process_type.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "partition_alloc/buildflags.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "ui/shell_dialogs/selected_file_info.h"

using heap_profiling::Mode;
using heap_profiling::ProfilingProcessHost;

namespace {

// Returns the string to display at the top of the page for help.
std::string GetMessageString() {
#if PA_BUILDFLAG(USE_ALLOCATOR_SHIM)
  Mode mode = Mode::kNone;
  if (heap_profiling::Supervisor::GetInstance()->HasStarted()) {
    mode = heap_profiling::Supervisor::GetInstance()->GetMode();
  }
  switch (mode) {
    case Mode::kAll:
      return std::string("Memory logging is enabled for all processes.");

    case Mode::kAllRenderers:
      return std::string("Memory logging is enabled for all renderers.");

    case Mode::kBrowser:
      return std::string(
          "Memory logging is enabled for just the browser process.");

    case Mode::kGpu:
      return std::string("Memory logging is enabled for just the gpu process.");

    case Mode::kMinimal:
      return std::string(
          "Memory logging is enabled for the browser and GPU processes.");

    case Mode::kRendererSampling:
      return std::string(
          "Memory logging is enabled for at most one renderer process. Each "
          "renderer process has a fixed probability of being sampled at "
          "startup.");

    case Mode::kUtilitySampling:
      return std::string(
          "Each utility process has a fixed probability of being profiled at "
          "startup.");

    case Mode::kNone:
    case Mode::kManual:
    default:
      return std::string(
          "Memory logging must be manually enabled for each process via "
          "chrome://memory-internals.");
  }
#elif defined(ADDRESS_SANITIZER)
  return "Memory logging is not available in this build because a memory "
         "sanitizer is running.";
#else
  return "Memory logging is not available in this build because "
         "USE_ALLOCATOR_SHIM is not set. It can not have sanitizers enabled "
         "and on Windows it must be a release non-component build.";
#endif
}

// Generates one row of the returned process info.
base::Value::List MakeProcessInfo(int pid, std::string description) {
  base::Value::List result;
  result.Append(pid);
  result.Append(std::move(description));
  return result;
}

// Some child processes have good descriptions and some don't, this function
// returns the best it can given the data.
std::string GetChildDescription(const content::ChildProcessData& data) {
  if (!data.name.empty()) {
    return base::UTF16ToUTF8(data.name);
  }
  return content::GetProcessTypeNameInEnglish(data.process_type);
}

void CreateAndAddMemoryInternalsUIHTMLSource(Profile* profile) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIMemoryInternalsHost);
  source->AddResourcePaths(base::make_span(kMemoryInternalsResources,
                                           kMemoryInternalsResourcesSize));
  source->SetDefaultResource(IDR_MEMORY_INTERNALS_MEMORY_INTERNALS_HTML);
}

class MemoryInternalsDOMHandler : public content::WebUIMessageHandler,
                                  ui::SelectFileDialog::Listener {
 public:
  explicit MemoryInternalsDOMHandler(content::WebUI* web_ui);

  MemoryInternalsDOMHandler(const MemoryInternalsDOMHandler&) = delete;
  MemoryInternalsDOMHandler& operator=(const MemoryInternalsDOMHandler&) =
      delete;

  ~MemoryInternalsDOMHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // Callback for the "requestProcessList" message.
  void HandleRequestProcessList(const base::Value::List& args);

  // Callback for the "saveDump" message.
  void HandleSaveDump(const base::Value::List& args);

  // Callback for the "startProfiling" message.
  void HandleStartProfiling(const base::Value::List& args);

 protected:
  // WebUIMessageHandler implementation.
  void OnJavascriptDisallowed() override;

 private:
  // Sends a request for a process list, and posts the result to
  // ReturnProcessListOnUIThread(). Takes ownership of `callback_id` so it can
  // be bound to the posted task without copying.
  void RequestProcessList(base::Value callback_id, bool success);

  void ReturnProcessListOnUIThread(const base::Value& callback_id,
                                   std::vector<base::Value::List> children,
                                   std::vector<base::ProcessId> profiled_pids);

  // SelectFileDialog::Listener implementation:
  void FileSelected(const ui::SelectedFileInfo& file, int index) override;
  void FileSelectionCanceled() override;

  void SaveTraceFinished(bool success);

  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;
#if !BUILDFLAG(IS_ANDROID)
  raw_ptr<content::WebUI> web_ui_;  // The WebUI that owns us.
#endif

  base::WeakPtrFactory<MemoryInternalsDOMHandler> weak_factory_{this};
};

MemoryInternalsDOMHandler::MemoryInternalsDOMHandler(content::WebUI* web_ui)
#if !BUILDFLAG(IS_ANDROID)
    : web_ui_(web_ui)
#endif
{
}

MemoryInternalsDOMHandler::~MemoryInternalsDOMHandler() {
  if (select_file_dialog_) {
    select_file_dialog_->ListenerDestroyed();
  }
}

void MemoryInternalsDOMHandler::RegisterMessages() {
  // Unretained should be OK here since this class is bound to the lifetime of
  // the WebUI.
  web_ui()->RegisterMessageCallback(
      "requestProcessList",
      base::BindRepeating(&MemoryInternalsDOMHandler::HandleRequestProcessList,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "saveDump",
      base::BindRepeating(&MemoryInternalsDOMHandler::HandleSaveDump,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "startProfiling",
      base::BindRepeating(&MemoryInternalsDOMHandler::HandleStartProfiling,
                          base::Unretained(this)));
}

void MemoryInternalsDOMHandler::HandleRequestProcessList(
    const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(args.size(), 1u);
  RequestProcessList(args[0].Clone(), /*success=*/true);
}

void MemoryInternalsDOMHandler::HandleSaveDump(const base::Value::List&) {
  base::FilePath default_file = base::FilePath().AppendASCII(
      base::StringPrintf("trace_with_heap_dump.json.gz"));

  AllowJavascript();

#if BUILDFLAG(IS_ANDROID)
  base::Value result("Saving...");
  FireWebUIListener("save-dump-progress", result);

  // On Android write to the user data dir.
  // TODO(bug 757115) Does it make sense to show the Android file picker here
  // instead? Need to test what that looks like.
  base::FilePath user_data_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  base::FilePath output_path = user_data_dir.Append(default_file);
  ProfilingProcessHost::GetInstance()->SaveTraceWithHeapDumpToFile(
      std::move(output_path),
      base::BindOnce(&MemoryInternalsDOMHandler::SaveTraceFinished,
                     weak_factory_.GetWeakPtr()),
      false);
#else
  if (select_file_dialog_) {
    return;  // Currently running, wait for existing save to complete.
  }
  select_file_dialog_ = ui::SelectFileDialog::Create(
      this,
      std::make_unique<ChromeSelectFilePolicy>(web_ui_->GetWebContents()));

  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_SAVEAS_FILE, std::u16string(), default_file,
      nullptr, 0, FILE_PATH_LITERAL(".json.gz"),
      web_ui_->GetWebContents()->GetTopLevelNativeWindow());
#endif
}

void MemoryInternalsDOMHandler::HandleStartProfiling(
    const base::Value::List& args) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  CHECK_EQ(args.size(), 2u);
  const base::Value& callback_id = args[0];
  const base::ProcessId pid = args[1].GetInt();

  // Refresh to get the updated state of the profiled process after profiling
  // starts.
  heap_profiling::mojom::ProfilingService::AddProfilingClientCallback
      refresh_callback = base::BindPostTaskToCurrentDefault(
          base::BindOnce(&MemoryInternalsDOMHandler::RequestProcessList,
                         weak_factory_.GetWeakPtr(), callback_id.Clone()));

  heap_profiling::Supervisor* supervisor =
      heap_profiling::Supervisor::GetInstance();
  if (supervisor->HasStarted()) {
    supervisor->StartManualProfiling(pid, std::move(refresh_callback));
  } else {
    supervisor->Start(base::BindOnce(
        &heap_profiling::Supervisor::StartManualProfiling,
        base::Unretained(supervisor), pid, std::move(refresh_callback)));
  }
}

void MemoryInternalsDOMHandler::OnJavascriptDisallowed() {
  // Cancel any callbacks that might trigger Javascript.
  weak_factory_.InvalidateWeakPtrs();
}

void MemoryInternalsDOMHandler::RequestProcessList(base::Value callback_id,
                                                   bool success) {
  if (!success) {
    return;
  }

  std::vector<base::Value::List> result;

  // The only non-renderer child processes that currently support out-of-process
  // heap profiling are GPU and UTILITY.
  for (content::BrowserChildProcessHostIterator iter; !iter.Done(); ++iter) {
    // Note that ChildProcessData.id is a child ID and not an OS PID.
    const content::ChildProcessData& data = iter.GetData();

    if (data.process_type == content::PROCESS_TYPE_GPU ||
        data.process_type == content::PROCESS_TYPE_UTILITY) {
      result.push_back(
          MakeProcessInfo(data.GetProcess().Pid(), GetChildDescription(data)));
    }
  }

  heap_profiling::Supervisor* supervisor =
      heap_profiling::Supervisor::GetInstance();

  // The supervisor hasn't started, so return an empty list.
  if (!supervisor->HasStarted()) {
    ReturnProcessListOnUIThread(std::move(callback_id), std::move(result),
                                std::vector<base::ProcessId>());
    return;
  }

  supervisor->GetProfiledPids(base::BindOnce(
      &MemoryInternalsDOMHandler::ReturnProcessListOnUIThread,
      weak_factory_.GetWeakPtr(), std::move(callback_id), std::move(result)));
}

void MemoryInternalsDOMHandler::ReturnProcessListOnUIThread(
    const base::Value& callback_id,
    std::vector<base::Value::List> children,
    std::vector<base::ProcessId> profiled_pids) {
  // This function will be called with the child processes that are not
  // renderers. It will fill in the browser and renderer processes on the UI
  // thread (RenderProcessHost is UI-thread only) and return the full list.
  base::Value::List process_list;

  // Add browser process.
  process_list.Append(MakeProcessInfo(base::GetCurrentProcId(), "Browser"));

  // Append renderer processes.
  auto iter = content::RenderProcessHost::AllHostsIterator();
  while (!iter.IsAtEnd()) {
    if (iter.GetCurrentValue()->GetProcess().IsValid()) {
      base::ProcessId renderer_pid = iter.GetCurrentValue()->GetProcess().Pid();
      if (renderer_pid != 0) {
        // TODO(brettw) make a better description of the process, maybe see
        // what TaskManager does to get the page title.
        process_list.Append(MakeProcessInfo(renderer_pid, "Renderer"));
      }
    }
    iter.Advance();
  }

  // Append all child processes collected on the IO thread.
  for (auto& child : children) {
    process_list.Append(std::move(child));
  }

  // Sort profiled_pids to allow binary_search in the loop.
  std::sort(profiled_pids.begin(), profiled_pids.end());

  // Append whether each process is being profiled.
  for (base::Value& value : process_list) {
    base::Value::List& list = value.GetList();
    DCHECK_EQ(list.size(), 2u);

    base::ProcessId pid = static_cast<base::ProcessId>(list[0].GetInt());
    bool is_profiled =
        std::binary_search(profiled_pids.begin(), profiled_pids.end(), pid);
    list.Append(is_profiled);
  }

  // Pass the results in a dictionary.
  base::Value::Dict result;
  result.Set("message", GetMessageString());
  result.Set("processes", std::move(process_list));

  ResolveJavascriptCallback(callback_id, result);
}

void MemoryInternalsDOMHandler::FileSelected(const ui::SelectedFileInfo& file,
                                             int index) {
  base::Value result("Saving...");
  FireWebUIListener("save-dump-progress", result);

  ProfilingProcessHost::GetInstance()->SaveTraceWithHeapDumpToFile(
      file.path(),
      base::BindOnce(&MemoryInternalsDOMHandler::SaveTraceFinished,
                     weak_factory_.GetWeakPtr()),
      false);
  select_file_dialog_ = nullptr;
}

void MemoryInternalsDOMHandler::FileSelectionCanceled() {
  select_file_dialog_ = nullptr;
}

void MemoryInternalsDOMHandler::SaveTraceFinished(bool success) {
  base::Value result(success ? "Save successful." : "Save failure.");
  FireWebUIListener("save-dump-progress", result);
}

}  // namespace

MemoryInternalsUI::MemoryInternalsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  web_ui->AddMessageHandler(
      std::make_unique<MemoryInternalsDOMHandler>(web_ui));

  CreateAndAddMemoryInternalsUIHTMLSource(Profile::FromWebUI(web_ui));
}

MemoryInternalsUI::~MemoryInternalsUI() {}
