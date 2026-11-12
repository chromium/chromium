// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/fake_gcm_server.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/containers/map_util.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/ref_counted.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "google_apis/gcm/engine/gservices_switches.h"
#include "google_apis/gcm/protocol/checkin.pb.h"
#include "google_apis/gcm/protocol/mcs.pb.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace gcm {

namespace {

struct AidLoginCredentials {
  uint64_t android_id = 0;
  uint64_t security_token = 0;
};

// Parses `Authorization: AidLogin <android_id>:<security_token>`.
std::optional<AidLoginCredentials> ParseAidLoginHeader(
    std::string_view authorization_header) {
  std::optional<std::string_view> auth_value = base::RemovePrefix(
      authorization_header, "AidLogin ", base::CompareCase::INSENSITIVE_ASCII);
  if (!auth_value.has_value()) {
    return std::nullopt;
  }
  const std::vector<std::string_view> auth_parts = base::SplitStringPiece(
      *auth_value, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  AidLoginCredentials credentials;
  if (auth_parts.size() != 2 ||
      !base::StringToUint64(auth_parts[0], &credentials.android_id) ||
      !base::StringToUint64(auth_parts[1], &credentials.security_token) ||
      credentials.android_id == 0) {
    return std::nullopt;
  }
  return credentials;
}

// Parses a URL-encoded form body (`key1=val1&key2=val2`) into a map.
base::flat_map<std::string, std::string> ExtractParamsFromRequest(
    std::string_view request_content) {
  constexpr base::UnescapeRule::Type kUnescapeRules =
      base::UnescapeRule::NORMAL | base::UnescapeRule::REPLACE_PLUS_WITH_SPACE;
  base::StringViewPairs raw_pairs;
  base::SplitStringIntoKeyValueViewPairs(request_content, '=', '&', &raw_pairs);
  base::flat_map<std::string, std::string> params;
  for (const auto& [raw_key, raw_val] : raw_pairs) {
    if (!raw_key.empty()) {
      params[base::UnescapeBinaryURLComponent(raw_key, kUnescapeRules)] =
          base::UnescapeBinaryURLComponent(raw_val, kUnescapeRules);
    }
  }
  return params;
}

std::string FindParamOrEmpty(
    const base::flat_map<std::string, std::string>& params,
    std::string_view key) {
  const std::string* value = base::FindOrNull(params, key);
  return value ? *value : std::string();
}

}  // namespace

// Thread-safe state shared between the caller thread and `EmbeddedTestServer`'s
// IO thread.
class FakeGCMServer::StateCore
    : public base::RefCountedThreadSafe<FakeGCMServer::StateCore> {
 public:
  struct ResolvedDataMessage {
    uint64_t android_id = 0;
    mcs_proto::DataMessageStanza stanza;
  };

  StateCore() = default;

  void InitializeStorageDir(const base::FilePath& storage_dir) {
    CHECK(!storage_dir.empty());
    base::AutoLock auto_lock(lock_);
    state_file_path_ = storage_dir.AppendASCII("fake_gcm_server_state.json");
    LoadStateFromDiskLocked();
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleHttpRequest(
      const net::test_server::HttpRequest& request) {
    const std::string path(request.GetURL().path());
    if (path != kCheckinPath && path != kRegisterPath) {
      return nullptr;
    }

    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    if (request.method != net::test_server::METHOD_POST) {
      response->set_code(net::HTTP_METHOD_NOT_ALLOWED);
      return response;
    }

    std::string response_content;
    if (path == kCheckinPath) {
      response->set_code(
          HandleCheckinRequest(request.content, response_content));
      response->set_content_type("application/x-protobuf");
    } else {
      const std::string* auth_header =
          base::FindOrNull(request.headers, "Authorization");
      response->set_code(
          HandleRegisterRequest(auth_header ? *auth_header : std::string_view(),
                                request.content, response_content));
      response->set_content_type("text/plain");
    }
    response->set_content(std::move(response_content));
    return response;
  }

  // Looks up `registration_token` and builds the target `ResolvedDataMessage`
  // if registered.
  std::optional<ResolvedDataMessage> BuildDataMessageForToken(
      std::string_view registration_token,
      std::string_view raw_data) {
    base::AutoLock auto_lock(lock_);
    const RegistrationRecord* record =
        base::FindOrNull(registrations_, registration_token);
    if (!record) {
      return std::nullopt;
    }

    ResolvedDataMessage result;
    result.android_id = record->android_id;
    result.stanza.set_from(record->sender_id);
    result.stanza.set_category(record->category);
    result.stanza.set_raw_data(raw_data);
    result.stanza.set_immediate_ack(true);
    result.stanza.set_sent(base::Time::Now().InMillisecondsSinceUnixEpoch());
    CHECK(!record->subtype.empty());
    mcs_proto::AppData* app_data = result.stanza.add_app_data();
    app_data->set_key("subtype");
    app_data->set_value(record->subtype);
    return result;
  }

 private:
  friend class base::RefCountedThreadSafe<StateCore>;

  struct RegistrationRecord {
    uint64_t android_id = 0;
    std::string category;
    std::string subtype;
    std::string sender_id;
    std::string instance_id;
  };

  ~StateCore() = default;

  net::HttpStatusCode HandleCheckinRequest(std::string_view request_content,
                                           std::string& response_content) {
    checkin_proto::AndroidCheckinRequest request;
    if (!request.ParseFromString(request_content) || request.id() < 0) {
      return net::HTTP_BAD_REQUEST;
    }

    base::AutoLock auto_lock(lock_);
    uint64_t android_id = base::checked_cast<uint64_t>(request.id());
    uint64_t security_token = 0;
    if (android_id == 0) {
      android_id = ++next_android_id_;
      security_token = 100000ULL + android_id;
      devices_[android_id] = security_token;
      WriteStateToDiskLocked();
    } else {
      const uint64_t* existing_token = base::FindOrNull(devices_, android_id);
      if (!existing_token || request.security_token() != *existing_token) {
        return net::HTTP_UNAUTHORIZED;
      }
      security_token = *existing_token;
    }

    checkin_proto::AndroidCheckinResponse response;
    response.set_stats_ok(true);
    response.set_android_id(android_id);
    response.set_security_token(security_token);
    response.set_time_msec(base::Time::Now().InMillisecondsSinceUnixEpoch());
    response_content = response.SerializeAsString();
    return net::HTTP_OK;
  }

  net::HttpStatusCode HandleRegisterRequest(
      std::string_view authorization_header,
      std::string_view request_content,
      std::string& response_content) {
    const std::optional<AidLoginCredentials> credentials =
        ParseAidLoginHeader(authorization_header);
    if (!credentials.has_value()) {
      return net::HTTP_UNAUTHORIZED;
    }

    const base::flat_map<std::string, std::string> params =
        ExtractParamsFromRequest(request_content);
    const std::string category = FindParamOrEmpty(params, "app");
    const std::string subtype = FindParamOrEmpty(params, "X-subtype");
    const std::string sender_id = FindParamOrEmpty(params, "sender");
    const std::string instance_id = FindParamOrEmpty(params, "appid");
    const bool is_delete = FindParamOrEmpty(params, "delete") == "true";

    // Only InstanceID requests (which always populate `X-subtype`, `sender`,
    // and `appid`) are supported; legacy `GCMDriver::Register()` /
    // `Unregister()` are deprecated.
    if (category.empty() || subtype.empty() || sender_id.empty() ||
        instance_id.empty()) {
      return net::HTTP_BAD_REQUEST;
    }

    base::AutoLock auto_lock(lock_);
    const uint64_t* existing_token =
        base::FindOrNull(devices_, credentials->android_id);
    if (!existing_token || *existing_token != credentials->security_token) {
      return net::HTTP_UNAUTHORIZED;
    }

    if (is_delete) {
      base::EraseIf(registrations_, [&](const auto& entry) {
        const RegistrationRecord& record = entry.second;
        const bool sender_matches =
            sender_id == "*" || record.sender_id == sender_id;
        return record.android_id == credentials->android_id &&
               record.category == category && record.subtype == subtype &&
               sender_matches;
      });
      WriteStateToDiskLocked();
      response_content = base::StrCat({"token=", subtype});
      return net::HTTP_OK;
    }

    // Check if an identical registration already exists for this device.
    for (const auto& [existing_reg_token, record] : registrations_) {
      if (record.android_id == credentials->android_id &&
          record.category == category && record.subtype == subtype &&
          record.sender_id == sender_id && record.instance_id == instance_id) {
        response_content = base::StrCat({"token=", existing_reg_token});
        return net::HTTP_OK;
      }
    }

    const std::string token =
        base::StrCat({"fcm_token_", base::NumberToString(++next_token_id_)});
    RegistrationRecord record{
        .android_id = credentials->android_id,
        .category = category,
        .subtype = subtype,
        .sender_id = sender_id,
        .instance_id = instance_id,
    };
    registrations_[token] = std::move(record);
    WriteStateToDiskLocked();

    response_content = base::StrCat({"token=", token});
    return net::HTTP_OK;
  }

  void LoadStateFromDiskLocked() EXCLUSIVE_LOCKS_REQUIRED(lock_) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::string json_string;
    // The state file does not exist on the first run before any state has been
    // persisted to disk.
    if (!base::ReadFileToString(state_file_path_, &json_string)) {
      return;
    }

    std::optional<base::DictValue> root =
        base::JSONReader::ReadDict(json_string, base::JSON_PARSE_RFC);
    CHECK(root.has_value());

    if (const std::string* next_android_id_str =
            root->FindString("next_android_id")) {
      base::StringToUint64(*next_android_id_str, &next_android_id_);
    }
    if (const std::string* next_token_id_str =
            root->FindString("next_token_id")) {
      base::StringToUint64(*next_token_id_str, &next_token_id_);
    }

    if (const base::DictValue* devices_dict = root->FindDict("devices")) {
      for (const auto [android_id_str, token_val] : *devices_dict) {
        uint64_t android_id = 0;
        uint64_t security_token = 0;
        if (base::StringToUint64(android_id_str, &android_id) &&
            token_val.is_string() &&
            base::StringToUint64(token_val.GetString(), &security_token)) {
          devices_[android_id] = security_token;
        }
      }
    }

    if (const base::DictValue* registrations_dict =
            root->FindDict("registrations")) {
      for (const auto [token, record_val] : *registrations_dict) {
        if (!record_val.is_dict()) {
          continue;
        }
        const base::DictValue& record_dict = record_val.GetDict();
        RegistrationRecord record;
        if (const std::string* android_id_str =
                record_dict.FindString("android_id")) {
          base::StringToUint64(*android_id_str, &record.android_id);
        }
        if (const std::string* category = record_dict.FindString("category")) {
          record.category = *category;
        }
        if (const std::string* subtype = record_dict.FindString("subtype")) {
          record.subtype = *subtype;
        }
        if (const std::string* sender_id =
                record_dict.FindString("sender_id")) {
          record.sender_id = *sender_id;
        }
        if (const std::string* instance_id =
                record_dict.FindString("instance_id")) {
          record.instance_id = *instance_id;
        }
        registrations_[token] = std::move(record);
      }
    }
  }

  void WriteStateToDiskLocked() const EXCLUSIVE_LOCKS_REQUIRED(lock_) {
    CHECK(!state_file_path_.empty());
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::DictValue root;
    root.Set("next_android_id", base::NumberToString(next_android_id_));
    root.Set("next_token_id", base::NumberToString(next_token_id_));

    base::DictValue devices_dict;
    for (const auto& [android_id, security_token] : devices_) {
      devices_dict.Set(base::NumberToString(android_id),
                       base::NumberToString(security_token));
    }
    root.Set("devices", std::move(devices_dict));

    base::DictValue registrations_dict;
    for (const auto& [token, record] : registrations_) {
      base::DictValue record_dict;
      record_dict.Set("android_id", base::NumberToString(record.android_id));
      record_dict.Set("category", record.category);
      record_dict.Set("subtype", record.subtype);
      record_dict.Set("sender_id", record.sender_id);
      record_dict.Set("instance_id", record.instance_id);
      registrations_dict.Set(token, std::move(record_dict));
    }
    root.Set("registrations", std::move(registrations_dict));

    std::string json_string;
    CHECK(base::JSONWriter::Write(root, &json_string));
    CHECK(base::CreateDirectory(state_file_path_.DirName()));
    CHECK(base::WriteFile(state_file_path_, json_string));
  }

  mutable base::Lock lock_;
  base::FilePath state_file_path_ GUARDED_BY(lock_);
  uint64_t next_android_id_ GUARDED_BY(lock_) = 1000;
  uint64_t next_token_id_ GUARDED_BY(lock_) = 0;
  base::flat_map<uint64_t, uint64_t> devices_ GUARDED_BY(lock_);
  base::flat_map<std::string, RegistrationRecord> registrations_
      GUARDED_BY(lock_);
};

FakeGCMServer::FakeGCMServer()
    : state_core_(base::MakeRefCounted<StateCore>()) {}

FakeGCMServer::~FakeGCMServer() = default;

void FakeGCMServer::InitializeAndListen() {
  mcs_server_.InitializeAndListen();
}

void FakeGCMServer::Start(const base::FilePath& storage_dir) {
  state_core_->InitializeStorageDir(storage_dir);
  mcs_server_.Start(storage_dir);
}

void FakeGCMServer::RegisterRequestHandler(
    net::test_server::EmbeddedTestServer& embedded_test_server) {
  embedded_test_server.RegisterRequestHandler(
      base::BindRepeating(&StateCore::HandleHttpRequest, state_core_));
}

void FakeGCMServer::AppendCommandLineSwitches(
    base::CommandLine* command_line,
    const net::test_server::EmbeddedTestServer& embedded_test_server) const {
  command_line->AppendSwitchASCII(
      switches::kGCMCheckinURL,
      embedded_test_server.GetURL(kCheckinPath).spec());
  command_line->AppendSwitchASCII(
      switches::kGCMRegistrationURL,
      embedded_test_server.GetURL(kRegisterPath).spec());
  command_line->AppendSwitchASCII(switches::kGCMMCSEndpoint,
                                  GetMCSEndpointURL().spec());
}

GURL FakeGCMServer::GetMCSEndpointURL() const {
  return mcs_server_.GetEndpointURL();
}

bool FakeGCMServer::SendMessage(std::string_view registration_token,
                                std::string_view raw_data) {
  std::optional<StateCore::ResolvedDataMessage> resolved =
      state_core_->BuildDataMessageForToken(registration_token, raw_data);
  if (!resolved.has_value()) {
    return false;
  }
  mcs_server_.SendDataMessage(resolved->android_id,
                              std::move(resolved->stanza));
  return true;
}

void FakeGCMServer::DisconnectAllClients() {
  mcs_server_.DisconnectAllClients();
}

}  // namespace gcm
