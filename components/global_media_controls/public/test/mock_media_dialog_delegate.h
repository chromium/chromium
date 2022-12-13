// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_TEST_MOCK_MEDIA_DIALOG_DELEGATE_H_
#define COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_TEST_MOCK_MEDIA_DIALOG_DELEGATE_H_

#include "components/global_media_controls/public/media_dialog_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace global_media_controls {
namespace test {

class MockMediaDialogDelegate : public MediaDialogDelegate {
 public:
  MockMediaDialogDelegate();
  MockMediaDialogDelegate(const MockMediaDialogDelegate&) = delete;
  MockMediaDialogDelegate& operator=(const MockMediaDialogDelegate&) = delete;
  ~MockMediaDialogDelegate() override;

  MOCK_METHOD(MediaItemUI*,
              ShowMediaItem,
              (const std::string&,
               base::WeakPtr<media_message_center::MediaNotificationItem>));
  MOCK_METHOD(void, HideMediaItem, (const std::string&));
  MOCK_METHOD(void,
              RefreshMediaItem,
              (const std::string&,
               base::WeakPtr<media_message_center::MediaNotificationItem>));
  MOCK_METHOD(void, HideMediaDialog, ());
  MOCK_METHOD(void, Focus, ());
};

}  // namespace test
}  // namespace global_media_controls

#endif  // COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_TEST_MOCK_MEDIA_DIALOG_DELEGATE_H_
