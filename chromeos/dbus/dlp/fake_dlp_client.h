// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_DLP_FAKE_DLP_CLIENT_H_
#define CHROMEOS_DBUS_DLP_FAKE_DLP_CLIENT_H_

#include <optional>
#include <string>

#include "base/containers/flat_map.h"
#include "base/observer_list.h"
#include "chromeos/dbus/dlp/dlp_client.h"
#include "chromeos/dbus/dlp/dlp_service.pb.h"
#include "dbus/object_proxy.h"

namespace chromeos {

class COMPONENT_EXPORT(DLP) FakeDlpClient : public DlpClient,
                                            public DlpClient::TestInterface {
 public:
  FakeDlpClient();
  FakeDlpClient(const FakeDlpClient&) = delete;
  FakeDlpClient& operator=(const FakeDlpClient&) = delete;
  ~FakeDlpClient() override;

  // DlpClient implementation:
  void SetDlpFilesPolicy(const dlp::SetDlpFilesPolicyRequest request,
                         SetDlpFilesPolicyCallback callback) override;
  void AddFiles(const dlp::AddFilesRequest request,
                AddFilesCallback callback) override;
  void GetFilesSources(const dlp::GetFilesSourcesRequest request,
                       GetFilesSourcesCallback callback) override;
  void CheckFilesTransfer(const dlp::CheckFilesTransferRequest request,
                          CheckFilesTransferCallback callback) override;
  void RequestFileAccess(const dlp::RequestFileAccessRequest request,
                         RequestFileAccessCallback callback) override;
  void GetDatabaseEntries(GetDatabaseEntriesCallback callback) override;
  bool IsAlive() const override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  bool HasObserver(const Observer* observer) const override;
  DlpClient::TestInterface* GetTestInterface() override;

  // DlpClient::TestInterface implementation:
  int GetSetDlpFilesPolicyCount() const override;
  void SetFakeSource(const std::string& fake_source) override;
  void SetCheckFilesTransferResponse(
      dlp::CheckFilesTransferResponse response) override;
  void SetFileAccessAllowed(bool allowed) override;
  void SetIsAlive(bool is_alive) override;
  void SetAddFilesMock(AddFilesCall mock) override;
  void SetGetFilesSourceMock(GetFilesSourceCall mock) override;
  dlp::CheckFilesTransferRequest GetLastCheckFilesTransferRequest()
      const override;
  void SetRequestFileAccessMock(RequestFileAccessCall mock) override;
  void SetCheckFilesTransferMock(CheckFilesTransferCall mock) override;

 private:
  int set_dlp_files_policy_count_ = 0;
  bool file_access_allowed_ = true;
  bool is_alive_ = true;
  // Map from file path to a pair of source_url and referrer_url.
  base::flat_map<std::string, std::pair<std::string, std::string>>
      files_database_;
  std::optional<std::string> fake_source_;
  std::optional<dlp::CheckFilesTransferResponse> check_files_transfer_response_;
  std::optional<AddFilesCall> add_files_mock_;
  std::optional<GetFilesSourceCall> get_files_source_mock_;
  dlp::CheckFilesTransferRequest last_check_files_transfer_request_;
  std::optional<RequestFileAccessCall> request_file_access_mock_;
  std::optional<CheckFilesTransferCall> check_files_transfer_mock_;
  base::ObserverList<Observer> observers_;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_DLP_FAKE_DLP_CLIENT_H_
