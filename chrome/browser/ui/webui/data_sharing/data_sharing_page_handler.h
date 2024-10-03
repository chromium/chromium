// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_DATA_SHARING_DATA_SHARING_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_DATA_SHARING_DATA_SHARING_PAGE_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/webui/data_sharing/data_sharing.mojom.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class DataSharingUI;
class GoogleServiceAuthError;
class Profile;

namespace signin {
struct AccessTokenInfo;
class AccessTokenFetcher;
}  // namespace signin

class DataSharingPageHandler : public data_sharing::mojom::PageHandler {
 public:
  DataSharingPageHandler(
      DataSharingUI* webui_controller,
      mojo::PendingReceiver<data_sharing::mojom::PageHandler> receiver,
      mojo::PendingRemote<data_sharing::mojom::Page> page);

  DataSharingPageHandler(const DataSharingPageHandler&) = delete;
  DataSharingPageHandler& operator=(const DataSharingPageHandler&) = delete;

  ~DataSharingPageHandler() override;

  void ShowUI() override;

  void CloseUI(int status_code) override;

  void ApiInitComplete() override;

  void GetShareLink(const std::string& group_id,
                    const std::string& access_token,
                    GetShareLinkCallback callback) override;

  void AssociateTabGroupWithGroupId(const std::string& tab_group_id,
                                    const std::string& group_id) override;

  void ReadGroups(std::vector<std::string> group_ids,
                  data_sharing::mojom::Page::ReadGroupsCallback callback);

  void DeleteGroup(std::string group_id,
                   data_sharing::mojom::Page::DeleteGroupCallback callback);

 private:
  Profile* GetProfile();

  void RequestAccessToken();

  void OnAccessTokenFetched(GoogleServiceAuthError error,
                            signin::AccessTokenInfo access_token_info);

  // webui_controller_ owns DataSharingPageHandler and outlives it.
  const raw_ptr<DataSharingUI> webui_controller_;
  std::unique_ptr<signin::AccessTokenFetcher> access_token_fetcher_;
  std::unique_ptr<base::OneShotTimer> access_token_refresh_timer_;

  mojo::Receiver<data_sharing::mojom::PageHandler> receiver_;
  mojo::Remote<data_sharing::mojom::Page> page_;

  bool api_initialized_ = false;

  base::WeakPtrFactory<DataSharingPageHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_DATA_SHARING_DATA_SHARING_PAGE_HANDLER_H_
