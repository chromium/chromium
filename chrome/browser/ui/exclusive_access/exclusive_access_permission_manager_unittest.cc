// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/exclusive_access/exclusive_access_permission_manager.h"

#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_request_description.h"
#include "content/public/browser/permission_result.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_permission_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"

class ExclusiveAccessPermissionManagerTest : public BrowserWithTestWindowTest {
 public:
  ExclusiveAccessPermissionManagerTest()
      : BrowserWithTestWindowTest(
            base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME),
        manager_(nullptr) {}
  ~ExclusiveAccessPermissionManagerTest() override = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    manager_.set_permission_controller_for_test(&permission_controller_);
    AddTab(browser(), GURL("https://example.com"));
  }

 protected:
  void QueuePointerLockRequest() {
    manager_.QueuePermissionRequest(
        blink::PermissionType::POINTER_LOCK,
        base::BindOnce(&base::MockOnceClosure::Run,
                       base::Unretained(&pointer_granted_callback_)),
        base::BindOnce(&base::MockOnceClosure::Run,
                       base::Unretained(&pointer_denied_callback_)),
        web_contents());
  }

  void QueueKeyboardLockRequest() {
    manager_.QueuePermissionRequest(
        blink::PermissionType::KEYBOARD_LOCK,
        base::BindOnce(&base::MockOnceClosure::Run,
                       base::Unretained(&keyboard_granted_callback_)),
        base::BindOnce(&base::MockOnceClosure::Run,
                       base::Unretained(&keyboard_denied_callback_)),
        web_contents());
  }

  void WaitForPermissionControllerResponse(
      std::optional<blink::mojom::PermissionStatus> pointer_lock_response,
      std::optional<blink::mojom::PermissionStatus> keyboard_lock_response) {
    EXPECT_CALL(permission_controller_, RequestPermissionsFromCurrentDocument)
        .WillRepeatedly(testing::WithArgs<1, 2>(testing::Invoke(
            [&](content::PermissionRequestDescription description,
                base::OnceCallback<void(
                    const std::vector<blink::mojom::PermissionStatus>&)>
                    callback) {
              switch (description.permissions.at(0)) {
                case blink::PermissionType::POINTER_LOCK:
                  if (pointer_lock_response) {
                    std::move(callback).Run({*pointer_lock_response});
                  }
                  break;
                case blink::PermissionType::KEYBOARD_LOCK:
                  if (keyboard_lock_response) {
                    std::move(callback).Run({*keyboard_lock_response});
                  }
                  break;
                default:
                  NOTREACHED_IN_MIGRATION();
              }
            })));
    task_environment()->FastForwardBy(base::Milliseconds(200));
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::MockPermissionController permission_controller_;
  ExclusiveAccessPermissionManager manager_;

  base::MockOnceClosure pointer_granted_callback_;
  base::MockOnceClosure pointer_denied_callback_;
  base::MockOnceClosure keyboard_granted_callback_;
  base::MockOnceClosure keyboard_denied_callback_;
};

TEST_F(ExclusiveAccessPermissionManagerTest, GrantPermission) {
  QueuePointerLockRequest();
  EXPECT_CALL(pointer_granted_callback_, Run);
  WaitForPermissionControllerResponse(
      /*pointer_lock_response=*/blink::mojom::PermissionStatus::GRANTED,
      /*keyboard_lock_response=*/std::nullopt);
}

TEST_F(ExclusiveAccessPermissionManagerTest, DenyPermission) {
  QueuePointerLockRequest();
  EXPECT_CALL(pointer_denied_callback_, Run);
  WaitForPermissionControllerResponse(
      /*pointer_lock_response=*/blink::mojom::PermissionStatus::DENIED,
      /*keyboard_lock_response=*/std::nullopt);
}

TEST_F(ExclusiveAccessPermissionManagerTest, HandleMultipleRequests) {
  QueuePointerLockRequest();
  // Fast-forwarding by less than the request delay (100ms) should not trigger
  // the request, and the request should get grouped with the next request.
  EXPECT_CALL(permission_controller_, RequestPermissionsFromCurrentDocument)
      .Times(0);
  task_environment()->FastForwardBy(base::Milliseconds(50));
  testing::Mock::VerifyAndClearExpectations(&permission_controller_);

  // Now, we fast-forward by more than the request delay, and the request
  // should be made.
  EXPECT_CALL(pointer_granted_callback_, Run);
  EXPECT_CALL(keyboard_denied_callback_, Run);
  QueueKeyboardLockRequest();
  WaitForPermissionControllerResponse(
      /*pointer_lock_response=*/blink::mojom::PermissionStatus::GRANTED,
      /*keyboard_lock_response=*/blink::mojom::PermissionStatus::DENIED);
}

TEST_F(ExclusiveAccessPermissionManagerTest,
       CloseTabBeforeRequestingPermission) {
  QueuePointerLockRequest();
  browser()->tab_strip_model()->CloseAllTabs();

  EXPECT_CALL(permission_controller_, RequestPermissionsFromCurrentDocument)
      .Times(0);
  task_environment()->FastForwardBy(base::Milliseconds(200));
}
