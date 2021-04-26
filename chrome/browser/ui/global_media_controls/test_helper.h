// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_TEST_HELPER_H_
#define CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_TEST_HELPER_H_

#include "chrome/browser/ui/global_media_controls/media_dialog_delegate.h"
#include "components/media_message_center/media_notification_item.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {
class WebContents;
}
class MediaNotificationService;

using media_message_center::MediaNotificationView;

class MockMediaNotificationItem
    : public media_message_center::MediaNotificationItem {
 public:
  MockMediaNotificationItem();
  ~MockMediaNotificationItem() final;

  base::WeakPtr<MockMediaNotificationItem> GetWeakPtr();

  MOCK_METHOD(void, SetView, (MediaNotificationView*));
  MOCK_METHOD(void,
              OnMediaSessionActionButtonPressed,
              (media_session::mojom::MediaSessionAction));
  MOCK_METHOD(void, SeekTo, (base::TimeDelta));
  MOCK_METHOD(void, Dismiss, ());
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
  MOCK_METHOD2(
      ShowMediaSession,
      MediaNotificationContainerImpl*(
          const std::string& id,
          base::WeakPtr<media_message_center::MediaNotificationItem> item));
  MOCK_METHOD1(HideMediaSession, void(const std::string& id));
  std::unique_ptr<OverlayMediaNotification> PopOut(const std::string& id,
                                                   gfx::Rect bounds) override;
  void HideMediaDialog() override;

 private:
  MediaNotificationService* service_;

  DISALLOW_COPY_AND_ASSIGN(MockMediaDialogDelegate);
};

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_TEST_HELPER_H_
