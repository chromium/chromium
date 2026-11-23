// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/password_manager/remote_actor_credential_sharing_extension.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "chrome/common/password_manager/remote_actor_credential_sharing.mojom.h"
#include "chrome/test/base/chrome_render_view_test.h"
#include "components/password_manager/core/common/remote_actor/remote_actor_features.h"
#include "content/public/renderer/render_frame.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-value.h"

namespace {

constexpr char kAllowedUrl[] = "https://gemini.google.com/app";

class FakeRemoteActorCredentialSharing
    : public chrome::mojom::RemoteActorCredentialSharing {
 public:
  void Bind(mojo::ScopedInterfaceEndpointHandle handle) {
    ++bind_count_;
    receiver_.reset();
    receiver_.Bind(
        mojo::PendingAssociatedReceiver<
            chrome::mojom::RemoteActorCredentialSharing>(std::move(handle)));
    receiver_.set_disconnect_handler(
        base::BindOnce(&FakeRemoteActorCredentialSharing::OnDisconnect,
                       base::Unretained(this)));
  }

  void RequestAgentAuthentication(
      const std::string& gaia_id,
      const std::string& domain,
      const std::string& task_id,
      RequestAgentAuthenticationCallback callback) override {
    ++request_count_;
    last_gaia_id_ = gaia_id;
    last_domain_ = domain;
    last_task_id_ = task_id;
    if (hold_callback_) {
      pending_callback_ = std::move(callback);
    } else {
      std::move(callback).Run(next_result_);
    }
  }

  void set_next_result(bool result) { next_result_ = result; }
  void set_hold_callback(bool hold) { hold_callback_ = hold; }

  void CompletePendingCallback(bool result) {
    ASSERT_TRUE(pending_callback_);
    std::move(pending_callback_).Run(result);
  }

  int bind_count() const { return bind_count_; }
  int disconnect_count() const { return disconnect_count_; }
  int request_count() const { return request_count_; }
  const std::string& last_gaia_id() const { return last_gaia_id_; }
  const std::string& last_domain() const { return last_domain_; }
  const std::string& last_task_id() const { return last_task_id_; }

 private:
  void OnDisconnect() { ++disconnect_count_; }

  mojo::AssociatedReceiver<chrome::mojom::RemoteActorCredentialSharing>
      receiver_{this};
  int bind_count_ = 0;
  int disconnect_count_ = 0;
  int request_count_ = 0;
  bool next_result_ = true;
  bool hold_callback_ = false;
  std::string last_gaia_id_;
  std::string last_domain_;
  std::string last_task_id_;
  RequestAgentAuthenticationCallback pending_callback_;
};

class RemoteActorCredentialSharingExtensionTest : public ChromeRenderViewTest {
 protected:
  // ChromeRenderViewTest:
  void RegisterMainFrameRemoteInterfaces() override {
    GetMainRenderFrame()
        ->GetRemoteAssociatedInterfaces()
        ->OverrideBinderForTesting(
            chrome::mojom::RemoteActorCredentialSharing::Name_,
            base::BindRepeating(&FakeRemoteActorCredentialSharing::Bind,
                                base::Unretained(&fake_service_)));
  }

  std::optional<bool> IsApiExposedInMainWorld() {
    int exposed = 0;
    if (!ExecuteJavaScriptAndReturnIntValue(
            u"typeof chrome !== 'undefined' && "
            u"typeof chrome.requestAgentAuthentication === 'function' ? 1 : 0;",
            &exposed)) {
      return std::nullopt;
    }
    return exposed == 1;
  }

  std::optional<bool> CallRequestAgentAuthentication(std::string_view gaia_id,
                                                     std::string_view domain,
                                                     std::string_view task_id) {
    ExecuteJavaScriptForTests(base::StrCat({
        "window.authResult = -1;"
        "chrome.requestAgentAuthentication('",
        gaia_id,
        "', '",
        domain,
        "', '",
        task_id,
        "', (ok) => { window.authResult = "
        "typeof ok === 'boolean' ? (ok ? 1 : 0) : -2; });",
    }));
    base::RunLoop().RunUntilIdle();

    int result = -1;
    if (!ExecuteJavaScriptAndReturnIntValue(u"window.authResult;", &result) ||
        (result != 0 && result != 1)) {
      return std::nullopt;
    }
    return result == 1;
  }

  base::test::ScopedFeatureList feature_list_{
      password_manager::features::kRemoteActorCredentialSharing};
  std::optional<base::AutoReset<std::optional<bool>>> site_lock_override_{
      RemoteActorCredentialSharingExtension::SetIsLockedToSiteForTesting(true)};
  FakeRemoteActorCredentialSharing fake_service_;
};

TEST_F(RemoteActorCredentialSharingExtensionTest,
       NotExposedWhenProcessNotLockedToSite) {
  // Clearing the override falls back to `blink::Platform::IsLockedToSite()`,
  // which returns false inside `ChromeRenderViewTest`.
  site_lock_override_.reset();
  LoadHTMLWithUrlOverride("<html><body></body></html>", kAllowedUrl);
  EXPECT_EQ(IsApiExposedInMainWorld(), false);
}

TEST_F(RemoteActorCredentialSharingExtensionTest,
       ExposedOnlyOnAllowedMainFrameOrigin) {
  constexpr char kDisallowedUrl[] = "https://example.com/";
  LoadHTMLWithUrlOverride("<html><body></body></html>", kDisallowedUrl);
  EXPECT_EQ(IsApiExposedInMainWorld(), false);

  LoadHTMLWithUrlOverride("<html><body></body></html>", kAllowedUrl);
  EXPECT_EQ(IsApiExposedInMainWorld(), true);
}

TEST_F(RemoteActorCredentialSharingExtensionTest,
       NotExposedInIsolatedWorldOrSubframe) {
  LoadHTMLWithUrlOverride(
      "<html><body><iframe id='child'></iframe></body></html>", kAllowedUrl);
  ASSERT_EQ(IsApiExposedInMainWorld(), true);

  // Isolated worlds (such as Chrome extension content scripts) must not see
  // `chrome.requestAgentAuthentication`.
  v8::Isolate* isolate = Isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Value> isolated_result =
      GetMainFrame()->ExecuteScriptInIsolatedWorldAndReturnValue(
          ISOLATED_WORLD_ID_EXTENSIONS,
          blink::WebScriptSource(
              "typeof chrome !== 'undefined' && "
              "typeof chrome.requestAgentAuthentication === 'function'"),
          blink::BackForwardCacheAware::kAllow);
  ASSERT_FALSE(isolated_result.IsEmpty());
  ASSERT_TRUE(isolated_result->IsBoolean());
  EXPECT_FALSE(isolated_result->BooleanValue(isolate));

  // Child iframe must not see `chrome.requestAgentAuthentication`.
  blink::WebFrame* child_frame = GetMainFrame()->FirstChild();
  ASSERT_NE(child_frame, nullptr);
  ASSERT_TRUE(child_frame->IsWebLocalFrame());
  v8::Local<v8::Value> subframe_result =
      child_frame->ToWebLocalFrame()->ExecuteScriptAndReturnValue(
          blink::WebScriptSource(
              "typeof chrome !== 'undefined' && "
              "typeof chrome.requestAgentAuthentication === 'function'"));
  ASSERT_FALSE(subframe_result.IsEmpty());
  ASSERT_TRUE(subframe_result->IsBoolean());
  EXPECT_FALSE(subframe_result->BooleanValue(isolate));
}

TEST_F(RemoteActorCredentialSharingExtensionTest,
       WithoutUserGestureResolvesFalseWithoutCallingMojo) {
  LoadHTMLWithUrlOverride("<html><body></body></html>", kAllowedUrl);
  ASSERT_EQ(IsApiExposedInMainWorld(), true);
  ASSERT_FALSE(GetMainFrame()->HasTransientUserActivation());

  EXPECT_EQ(CallRequestAgentAuthentication("gaia", "example.com", "task"),
            false);
  EXPECT_EQ(fake_service_.bind_count(), 0);
  EXPECT_EQ(fake_service_.request_count(), 0);
}

TEST_F(RemoteActorCredentialSharingExtensionTest,
       WithUserGestureCallsMojoAndResetsOnNewDocument) {
  LoadHTMLWithUrlOverride("<html><body></body></html>", kAllowedUrl);
  ASSERT_EQ(IsApiExposedInMainWorld(), true);

  GetMainFrame()->NotifyUserActivation(
      blink::mojom::UserActivationNotificationType::kTest);
  EXPECT_EQ(CallRequestAgentAuthentication("gaia_1", "example.com", "task_1"),
            true);
  EXPECT_EQ(fake_service_.bind_count(), 1);
  EXPECT_EQ(fake_service_.request_count(), 1);
  EXPECT_EQ(fake_service_.last_gaia_id(), "gaia_1");
  EXPECT_EQ(fake_service_.last_domain(), "example.com");
  EXPECT_EQ(fake_service_.last_task_id(), "task_1");

  // A second request in the same document reuses the cached Mojo remote and
  // propagates a `false` reply.
  fake_service_.set_next_result(false);
  GetMainFrame()->NotifyUserActivation(
      blink::mojom::UserActivationNotificationType::kTest);
  EXPECT_EQ(CallRequestAgentAuthentication("gaia_2", "example.com", "task_2"),
            false);
  EXPECT_EQ(fake_service_.bind_count(), 1);
  EXPECT_EQ(fake_service_.request_count(), 2);

  // Navigating to a new document resets `remote_interface_`, so a subsequent
  // request re-binds the Mojo interface.
  fake_service_.set_next_result(true);
  LoadHTMLWithUrlOverride("<html><body>new doc</body></html>", kAllowedUrl);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(fake_service_.disconnect_count(), 1);

  GetMainFrame()->NotifyUserActivation(
      blink::mojom::UserActivationNotificationType::kTest);
  EXPECT_EQ(CallRequestAgentAuthentication("gaia_3", "example.com", "task_3"),
            true);
  EXPECT_EQ(fake_service_.bind_count(), 2);
  EXPECT_EQ(fake_service_.request_count(), 3);
}

TEST_F(RemoteActorCredentialSharingExtensionTest,
       IgnoresCallbackFromDifferentRealm) {
  // Cross-realm callback (function created in an iframe) must not execute.
  LoadHTMLWithUrlOverride(
      "<html><body><iframe id='child'></iframe></body></html>", kAllowedUrl);
  ASSERT_EQ(IsApiExposedInMainWorld(), true);

  GetMainFrame()->NotifyUserActivation(
      blink::mojom::UserActivationNotificationType::kTest);
  ExecuteJavaScriptForTests(
      "window.callbackRan = 0;"
      "const childFn = document.getElementById('child').contentWindow.Function("
      "    'window.parent.callbackRan = 1;');"
      "chrome.requestAgentAuthentication('gaia', 'example.com', 'task', "
      "    childFn);");
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(fake_service_.request_count(), 1);
  int callback_ran = -1;
  ASSERT_TRUE(ExecuteJavaScriptAndReturnIntValue(u"window.callbackRan;",
                                                 &callback_ran));
  EXPECT_EQ(callback_ran, 0);
}

TEST_F(RemoteActorCredentialSharingExtensionTest,
       ResetsMojoPipeAndDropsPendingCallbackOnNavigation) {
  // Navigating to a new document calls `DidCreateNewDocument()`, which resets
  // `remote_interface_` and disconnects the Mojo pipe before any pending reply
  // completes.
  LoadHTMLWithUrlOverride("<html><body></body></html>", kAllowedUrl);
  ASSERT_EQ(IsApiExposedInMainWorld(), true);

  fake_service_.set_hold_callback(true);
  GetMainFrame()->NotifyUserActivation(
      blink::mojom::UserActivationNotificationType::kTest);
  ExecuteJavaScriptForTests(
      "chrome.requestAgentAuthentication('gaia', 'example.com', 'task_stale', "
      "    () => {});");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(fake_service_.request_count(), 1);
  EXPECT_EQ(fake_service_.disconnect_count(), 0);

  LoadHTMLWithUrlOverride("<html><body>navigated</body></html>", kAllowedUrl);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(fake_service_.disconnect_count(), 1);

  // Completing the orphaned callback after pipe disconnect must not crash.
  fake_service_.CompletePendingCallback(true);
  base::RunLoop().RunUntilIdle();
}

}  // namespace
