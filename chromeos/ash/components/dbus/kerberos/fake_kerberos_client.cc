// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chromeos/ash/components/dbus/kerberos/fake_kerberos_client.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_split.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "third_party/cros_system_api/dbus/kerberos/dbus-constants.h"

namespace ash {
namespace {

// Fake validity lifetime for TGTs.
constexpr base::TimeDelta kTgtValidity = base::Hours(10);

// Fake renewal lifetime for TGTs.
constexpr base::TimeDelta kTgtRenewal = base::Hours(24);

// Blocklist for fake config validation.
const char* const kBlocklistedConfigOptions[] = {
    "allow_weak_crypto",
    "ap_req_checksum_type",
    "ccache_type",
    "default_ccache_name ",
    "default_client_keytab_name",
    "default_keytab_name",
    "default_realm",
    "k5login_authoritative",
    "k5login_directory",
    "kdc_req_checksum_type",
    "plugin_base_dir",
    "realm_try_domains",
    "safe_checksum_type",
    "verify_ap_req_nofail",
    "default_domain",
    "v4_instance_convert",
    "v4_realm",
    "[appdefaults]",
    "[plugins]",
};

// Performs a fake validation of a config line by just checking for some
// non-allowlisted keywords. Returns true if no blocklisted items are contained.
bool ValidateConfigLine(const std::string& line) {
  for (const char* option : kBlocklistedConfigOptions) {
    if (base::Contains(line, option)) {
      return false;
    }
  }
  return true;
}

// Runs ValidateConfigLine() on every line of |krb5_config|. Returns a
// ConfigErrorInfo object that indicates the first line where validation fails,
// if any.
kerberos::ConfigErrorInfo ValidateConfigLines(const std::string& krb5_config) {
  std::vector<std::string> lines = base::SplitString(
      krb5_config, "\r\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  for (size_t line_index = 0; line_index < lines.size(); ++line_index) {
    if (!ValidateConfigLine(lines[line_index])) {
      kerberos::ConfigErrorInfo error_info;
      error_info.set_code(kerberos::CONFIG_ERROR_KEY_NOT_SUPPORTED);
      error_info.set_line_index(static_cast<int>(line_index));
      return error_info;
    }
  }

  kerberos::ConfigErrorInfo error_info;
  error_info.set_code(kerberos::CONFIG_ERROR_NONE);
  return error_info;
}

// Posts |callback| on the current thread's task runner, passing it the
// |response| message.
template <class TProto>
void PostProtoResponse(base::OnceCallback<void(const TProto&)> callback,
                       const TProto& response,
                       base::TimeDelta delay) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(std::move(callback), response), delay);
}

// Similar to PostProtoResponse(), but posts |callback| with a proto containing
// only the given |error|.
template <class TProto>
void PostResponse(base::OnceCallback<void(const TProto&)> callback,
                  kerberos::ErrorType error,
                  base::TimeDelta delay) {
  TProto response;
  response.set_error(error);
  PostProtoResponse(std::move(callback), response, delay);
}

// Reads the password from the file descriptor |password_fd|.
// Not very efficient, but simple!
std::string ReadPassword(int password_fd) {
  std::string password;
  char c;
  while (base::ReadFromFD(password_fd, base::span_from_ref(c))) {
    password.push_back(c);
  }
  return password;
}

}  // namespace

FakeKerberosClient::FakeKerberosClient() = default;

FakeKerberosClient::~FakeKerberosClient() = default;

void FakeKerberosClient::AddAccount(const kerberos::AddAccountRequest& request,
                                    AddAccountCallback callback) {
  MaybeRecordFunctionCallForTesting(__FUNCTION__);
  auto it =
      base::ranges::find(accounts_, AccountData(request.principal_name()));
  if (it != accounts_.end()) {
    it->is_managed |= request.is_managed();
    PostResponse(std::move(callback), kerberos::ERROR_DUPLICATE_PRINCIPAL_NAME,
                 task_delay_);
    return;
  }

  AccountData data(request.principal_name());
  data.is_managed = request.is_managed();
  accounts_.push_back(data);
  PostResponse(std::move(callback), kerberos::ERROR_NONE, task_delay_);
}

void FakeKerberosClient::RemoveAccount(
    const kerberos::RemoveAccountRequest& request,
    RemoveAccountCallback callback) {
  MaybeRecordFunctionCallForTesting(__FUNCTION__);
  kerberos::RemoveAccountResponse response;
  auto it =
      base::ranges::find(accounts_, AccountData(request.principal_name()));
  if (it == accounts_.end()) {
    response.set_error(kerberos::ERROR_UNKNOWN_PRINCIPAL_NAME);
  } else {
    accounts_.erase(it);
    response.set_error(kerberos::ERROR_NONE);
  }

  MapAccountData(response.mutable_accounts());
  PostProtoResponse(std::move(callback), response, task_delay_);
}

void FakeKerberosClient::ClearAccounts(
    const kerberos::ClearAccountsRequest& request,
    ClearAccountsCallback callback) {
  MaybeRecordFunctionCallForTesting(__FUNCTION__);
  std::unordered_set<std::string> keep_list(
      request.principal_names_to_ignore_size());
  for (int n = 0; n < request.principal_names_to_ignore_size(); ++n)
    keep_list.insert(request.principal_names_to_ignore(n));

  for (auto it = accounts_.begin(); it != accounts_.end(); /* empty */) {
    if (base::Contains(keep_list, it->principal_name)) {
      ++it;
      continue;
    }

    switch (DetermineWhatToRemove(request.mode(), *it)) {
      case WhatToRemove::kNothing:
        ++it;
        continue;

      case WhatToRemove::kPassword:
        it->password.clear();
        ++it;
        continue;

      case WhatToRemove::kAccount:
        it = accounts_.erase(it);
        continue;
    }
  }

  kerberos::ClearAccountsResponse response;
  MapAccountData(response.mutable_accounts());
  response.set_error(kerberos::ERROR_NONE);
  PostProtoResponse(std::move(callback), response, task_delay_);
}

void FakeKerberosClient::ListAccounts(
    const kerberos::ListAccountsRequest& request,
    ListAccountsCallback callback) {
  MaybeRecordFunctionCallForTesting(__FUNCTION__);
  kerberos::ListAccountsResponse response;
  MapAccountData(response.mutable_accounts());
  response.set_error(kerberos::ERROR_NONE);
  PostProtoResponse(std::move(callback), response, task_delay_);
}

void FakeKerberosClient::SetConfig(const kerberos::SetConfigRequest& request,
                                   SetConfigCallback callback) {
  MaybeRecordFunctionCallForTesting(__FUNCTION__);
  AccountData* data = GetAccountData(request.principal_name());
  if (!data) {
    PostResponse(std::move(callback), kerberos::ERROR_UNKNOWN_PRINCIPAL_NAME,
                 task_delay_);
    return;
  }

  kerberos::ConfigErrorInfo error_info =
      ValidateConfigLines(request.krb5conf());
  if (error_info.code() != kerberos::CONFIG_ERROR_NONE) {
    PostResponse(std::move(callback), kerberos::ERROR_BAD_CONFIG, task_delay_);
    return;
  }

  data->krb5conf = request.krb5conf();
  PostResponse(std::move(callback), kerberos::ERROR_NONE, task_delay_);
}

void FakeKerberosClient::ValidateConfig(
    const kerberos::ValidateConfigRequest& request,
    ValidateConfigCallback callback) {
  MaybeRecordFunctionCallForTesting(__FUNCTION__);

  kerberos::ConfigErrorInfo error_info =
      ValidateConfigLines(request.krb5conf());
  kerberos::ValidateConfigResponse response;
  response.set_error(error_info.code() != kerberos::CONFIG_ERROR_NONE
                         ? kerberos::ERROR_BAD_CONFIG
                         : kerberos::ERROR_NONE);
  *response.mutable_error_info() = std::move(error_info);
  PostProtoResponse(std::move(callback), response, task_delay_);
}

void FakeKerberosClient::AcquireKerberosTgt(
    const kerberos::AcquireKerberosTgtRequest& request,
    int password_fd,
    AcquireKerberosTgtCallback callback) {
  MaybeRecordFunctionCallForTesting(__FUNCTION__);
  AccountData* data = GetAccountData(request.principal_name());
  if (!data) {
    PostResponse(std::move(callback), kerberos::ERROR_UNKNOWN_PRINCIPAL_NAME,
                 task_delay_);
    return;
  }

  // Remember whether to use the login password.
  data->use_login_password = request.use_login_password();

  std::string password;
  if (request.use_login_password()) {
    // "Retrieve" login password.
    password = "fake_login_password";

    // Erase a previously remembered password.
    data->password.clear();
  } else {
    // Remember password.
    password = ReadPassword(password_fd);
    if (!password.empty() && request.remember_password())
      data->password = password;

    // Use remembered password.
    if (password.empty())
      password = data->password;

    // Erase a previously remembered password.
    if (!request.remember_password())
      data->password.clear();
  }

  // Reject empty passwords.
  if (password.empty()) {
    PostResponse(std::move(callback), kerberos::ERROR_BAD_PASSWORD,
                 task_delay_);
    return;
  }

  if (simulated_number_of_network_failures_ > 0) {
    simulated_number_of_network_failures_--;
    PostResponse(std::move(callback), kerberos::ERROR_NETWORK_PROBLEM,
                 task_delay_);
    return;
  }

  // It worked! Magic!
  data->has_tgt = true;
  PostResponse(std::move(callback), kerberos::ERROR_NONE, task_delay_);
}

void FakeKerberosClient::GetKerberosFiles(
    const kerberos::GetKerberosFilesRequest& request,
    GetKerberosFilesCallback callback) {
  MaybeRecordFunctionCallForTesting(__FUNCTION__);
  AccountData* data = GetAccountData(request.principal_name());
  if (!data) {
    PostResponse(std::move(callback), kerberos::ERROR_UNKNOWN_PRINCIPAL_NAME,
                 task_delay_);
    return;
  }

  kerberos::GetKerberosFilesResponse response;
  if (data->has_tgt) {
    response.mutable_files()->set_krb5cc("Fake Kerberos credential cache");
    response.mutable_files()->set_krb5conf("Fake Kerberos configuration");
  }
  response.set_error(kerberos::ERROR_NONE);
  PostProtoResponse(std::move(callback), response, task_delay_);
}

base::CallbackListSubscription
FakeKerberosClient::SubscribeToKerberosFileChangedSignal(
    KerberosFilesChangedCallback callback) {
  MaybeRecordFunctionCallForTesting(__FUNCTION__);
  DCHECK(callback);
  return base::CallbackListSubscription();
}

base::CallbackListSubscription
FakeKerberosClient::SubscribeToKerberosTicketExpiringSignal(
    KerberosTicketExpiringCallback callback) {
  MaybeRecordFunctionCallForTesting(__FUNCTION__);
  DCHECK(callback);
  return base::CallbackListSubscription();
}

void FakeKerberosClient::SetTaskDelay(base::TimeDelta delay) {
  task_delay_ = delay;
}

void FakeKerberosClient::StartRecordingFunctionCalls() {
  DCHECK(!recorded_function_calls_);
  recorded_function_calls_ = std::string();
}

std::string FakeKerberosClient::StopRecordingAndGetRecordedFunctionCalls() {
  DCHECK(recorded_function_calls_);
  std::string result;
  recorded_function_calls_->swap(result);
  recorded_function_calls_.reset();
  return result;
}

std::size_t FakeKerberosClient::GetNumberOfAccounts() const {
  return accounts_.size();
}

void FakeKerberosClient::SetSimulatedNumberOfNetworkFailures(
    int number_of_failures) {
  simulated_number_of_network_failures_ = number_of_failures;
}

void FakeKerberosClient::MaybeRecordFunctionCallForTesting(
    const char* function_name) {
  if (!recorded_function_calls_)
    return;

  if (!recorded_function_calls_->empty())
    recorded_function_calls_->append(",");
  recorded_function_calls_->append(function_name);
}

KerberosClient::TestInterface* FakeKerberosClient::GetTestInterface() {
  return this;
}

FakeKerberosClient::AccountData* FakeKerberosClient::GetAccountData(
    const std::string& principal_name) {
  auto it = base::ranges::find(accounts_, AccountData(principal_name));
  return it != accounts_.end() ? &*it : nullptr;
}

FakeKerberosClient::AccountData::AccountData(const std::string& principal_name)
    : principal_name(principal_name) {}

FakeKerberosClient::AccountData::AccountData(const AccountData& other) =
    default;

FakeKerberosClient::AccountData& FakeKerberosClient::AccountData::operator=(
    const AccountData& other) = default;

bool FakeKerberosClient::AccountData::operator==(
    const AccountData& other) const {
  return principal_name == other.principal_name;
}

bool FakeKerberosClient::AccountData::operator!=(
    const AccountData& other) const {
  return !(*this == other);
}

void FakeKerberosClient::MapAccountData(RepeatedAccountField* accounts) {
  for (const AccountData& data : accounts_) {
    kerberos::Account* account = accounts->Add();
    account->set_principal_name(data.principal_name);
    account->set_krb5conf(data.krb5conf);
    account->set_tgt_validity_seconds(data.has_tgt ? kTgtValidity.InSeconds()
                                                   : 0);
    account->set_tgt_renewal_seconds(data.has_tgt ? kTgtRenewal.InSeconds()
                                                  : 0);
    account->set_is_managed(data.is_managed);
    account->set_password_was_remembered(!data.password.empty());
    account->set_use_login_password(data.use_login_password);
  }
}

// static
FakeKerberosClient::WhatToRemove FakeKerberosClient::DetermineWhatToRemove(
    kerberos::ClearMode mode,
    const AccountData& data) {
  switch (mode) {
    case kerberos::CLEAR_ALL:
      return WhatToRemove::kAccount;

    case kerberos::CLEAR_ONLY_MANAGED_ACCOUNTS:
      return data.is_managed ? WhatToRemove::kAccount : WhatToRemove::kNothing;

    case kerberos::CLEAR_ONLY_UNMANAGED_ACCOUNTS:
      return !data.is_managed ? WhatToRemove::kAccount : WhatToRemove::kNothing;

    case kerberos::CLEAR_ONLY_UNMANAGED_REMEMBERED_PASSWORDS:
      return !data.is_managed ? WhatToRemove::kPassword
                              : WhatToRemove::kNothing;
  }
  return WhatToRemove::kNothing;
}

}  // namespace ash
