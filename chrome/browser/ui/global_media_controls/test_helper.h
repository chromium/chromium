// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_TEST_HELPER_H_
#define CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_TEST_HELPER_H_

#include "chrome/browser/ui/global_media_controls/media_dialog_delegate.h"
#include "chrome/browser/ui/global_media_controls/media_items_manager.h"
#include "components/media_message_center/media_notification_item.h"
#include "components/media_router/browser/presentation/web_contents_presentation_manager.h"
#include "content/public/browser/presentation_request.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {
class WebContents;
}
class MediaNotificationService;

using media_message_center::MediaNotificationView;
using media_router::WebContentsPresentationManager;

class MockMediaNotificationItem
    : public media_message_center::MediaNotificationItem {
 public:
  MockMediaNotificationItem();
  ~MockMediaNotificationItem() override;

  base::WeakPtr<MockMediaNotificationItem> GetWeakPtr();

  MOCK_METHOD(void, SetView, (MediaNotificationView*));
  MOCK_METHOD(void,
              OnMediaSessionActionButtonPressed,
              (media_session::mojom::MediaSessionAction));
  MOCK_METHOD(void, SeekTo, (base::TimeDelta));
  MOCK_METHOD(void, Dismiss, ());
  MOCK_METHOD(void, SetVolume, (float));
  MOCK_METHOD(void, SetMute, (bool));
  MOCK_METHOD(media_message_center::SourceType, SourceType, ());

 private:
  base::WeakPtrFactory<MockMediaNotificationItem> weak_ptr_factory_{this};
};

class MockMediaDialogDelegate : public MediaDialogDelegate {
 public:
  MockMediaDialogDelegate();
  ~MockMediaDialogDelegate() override;

  void Open(MediaNotificationService* service);
  void OpenForWebContents(MediaNotificationService* service,
                          content::WebContents* content);
  void Close();
  // Need to use a proxy since std::unique_ptr is not copyable.
  MOCK_METHOD2(PopOutProxy,
               OverlayMediaNotification*(const std::string& id,
                                         gfx::Rect bounds));

  // MediaDialogDelegate implementation.
  MOCK_METHOD(
      MediaNotificationContainerImpl*,
      ShowMediaSession,
      (const std::string& id,
       base::WeakPtr<media_message_center::MediaNotificationItem> item));
  MOCK_METHOD(void, HideMediaSession, (const std::string& id));
  MOCK_METHOD(void, Focus, ());

  std::unique_ptr<OverlayMediaNotification> PopOut(const std::string& id,
                                                   gfx::Rect bounds) override;
  void HideMediaDialog() override;

 private:
  MediaNotificationService* service_;

  DISALLOW_COPY_AND_ASSIGN(MockMediaDialogDelegate);
};

class MockMediaItemsManager : public MediaItemsManager {
 public:
  MockMediaItemsManager();
  ~MockMediaItemsManager() override;

  MOCK_METHOD(void, ShowItem, (const std::string&));
  MOCK_METHOD(void, HideItem, (const std::string&));
};

class MockWebContentsPresentationManager
    : public WebContentsPresentationManager {
 public:
  MockWebContentsPresentationManager();
  ~MockWebContentsPresentationManager() override;

  void NotifyMediaRoutesChanged(
      const std::vector<media_router::MediaRoute>& routes);
  void SetDefaultPresentationRequest(
      const content::PresentationRequest& request);

  // WebContentsPresentationManager implementation.
  bool HasDefaultPresentationRequest() const override;
  const content::PresentationRequest& GetDefaultPresentationRequest()
      const override;
  void AddObserver(WebContentsPresentationManager::Observer* observer) override;
  void RemoveObserver(
      WebContentsPresentationManager::Observer* observer) override;
  base::WeakPtr<WebContentsPresentationManager> GetWeakPtr() override;

  MOCK_METHOD(void,
              OnPresentationResponse,
              (const content::PresentationRequest&,
               media_router::mojom::RoutePresentationConnectionPtr,
               const media_router::RouteRequestResult&));
  MOCK_METHOD(std::vector<media_router::MediaRoute>, GetMediaRoutes, ());

 private:
  absl::optional<content::PresentationRequest> default_presentation_request_;
  base::ObserverList<WebContentsPresentationManager::Observer> observers_;
  base::WeakPtrFactory<MockWebContentsPresentationManager> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_TEST_HELPER_H_
