// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/internal/android/android_browser_window.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/check_deref.h"
#include "base/memory/weak_ptr.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/blocked_content/chrome_popup_navigation_delegate.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window/internal/jni/AndroidBrowserWindow_jni.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/blocked_content/popup_blocker.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/browser/extensions/extension_browser_window_helper.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)

namespace {
using base::android::AttachCurrentThread;
using base::android::JavaRef;

}  // namespace

// Implements Java |AndroidBrowserWindow.Natives#create|.
static int64_t JNI_AndroidBrowserWindow_Create(JNIEnv* env,
                                               const JavaRef<jobject>& caller,
                                               int32_t browser_window_type,
                                               Profile* profile) {
  return reinterpret_cast<intptr_t>(new AndroidBrowserWindow(
      env, caller,
      static_cast<BrowserWindowInterface::Type>(browser_window_type), profile));
}

AndroidBrowserWindow::AndroidBrowserWindow(
    JNIEnv* env,
    const JavaRef<jobject>& java_android_browser_window,
    const BrowserWindowInterface::Type type,
    Profile* profile)
    : type_(type),
      profile_(CHECK_DEREF(profile)),
      session_id_(SessionID::NewUnique()) {
  java_android_browser_window_.Reset(env, java_android_browser_window);

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  extension_browser_window_helper_ =
      std::make_unique<extensions::ExtensionBrowserWindowHelper>(this, profile);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)
}

AndroidBrowserWindow::~AndroidBrowserWindow() {
  Java_AndroidBrowserWindow_clearNativePtr(AttachCurrentThread(),
                                           java_android_browser_window_);
}

void AndroidBrowserWindow::Destroy(JNIEnv* env) {
  delete this;
}

int32_t AndroidBrowserWindow::GetSessionIdForTesting(JNIEnv* env) const {
  return GetSessionID().id();
}

ui::UnownedUserDataHost& AndroidBrowserWindow::GetUnownedUserDataHost() {
  return unowned_user_data_host_;
}

const ui::UnownedUserDataHost& AndroidBrowserWindow::GetUnownedUserDataHost()
    const {
  return unowned_user_data_host_;
}

ui::BaseWindow* AndroidBrowserWindow::GetWindow() {
  return reinterpret_cast<ui::BaseWindow*>(
      Java_AndroidBrowserWindow_getOrCreateNativeBaseWindowPtr(
          AttachCurrentThread(), java_android_browser_window_));
}

const ui::BaseWindow* AndroidBrowserWindow::GetWindow() const {
  return reinterpret_cast<ui::BaseWindow*>(
      Java_AndroidBrowserWindow_getOrCreateNativeBaseWindowPtr(
          AttachCurrentThread(), java_android_browser_window_));
}

Profile* AndroidBrowserWindow::GetProfile() {
  return const_cast<Profile*>(
      static_cast<const AndroidBrowserWindow*>(this)->GetProfile());
}

const Profile* AndroidBrowserWindow::GetProfile() const {
  return &profile_.get();
}

const SessionID& AndroidBrowserWindow::GetSessionID() const {
  return session_id_;
}

bool AndroidBrowserWindow::IsDeleteScheduled() const {
  return Java_AndroidBrowserWindow_isDeleteScheduled(
      AttachCurrentThread(), java_android_browser_window_);
}

base::CallbackListSubscription AndroidBrowserWindow::RegisterBrowserDidClose(
    BrowserDidCloseCallback callback) {
  return browser_did_close_callback_list_.Add(std::move(callback));
}

BrowserWindowInterface::Type AndroidBrowserWindow::GetType() const {
  return type_;
}

base::WeakPtr<BrowserWindowInterface> AndroidBrowserWindow::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

content::WebContents* AndroidBrowserWindow::OpenURL(
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  NavigateParams nav_params(this, params.url, params.transition);
  nav_params.FillNavigateParamsFromOpenURLParams(params);
  if (params.user_gesture) {
    nav_params.window_action = NavigateParams::WindowAction::kShowWindow;
  }

  content::WebContents* source = content::WebContents::FromRenderFrameHost(
      content::RenderFrameHost::FromID(params.source_render_process_id,
                                       params.source_render_frame_id));

  bool is_popup =
      source && blocked_content::ConsiderForPopupBlocking(params.disposition);
  auto popup_delegate =
      std::make_unique<ChromePopupNavigationDelegate>(std::move(nav_params));
  if (is_popup) {
    popup_delegate.reset(static_cast<ChromePopupNavigationDelegate*>(
        blocked_content::MaybeBlockPopup(
            source, nullptr, std::move(popup_delegate), &params,
            blink::mojom::WindowFeatures(),
            HostContentSettingsMapFactory::GetForProfile(
                source->GetBrowserContext()))
            .release()));
    if (!popup_delegate) {
      return nullptr;
    }
  }

  NavigateParams* params_ptr = popup_delegate->nav_params();

  // Try to handle the request synchronously first.
  base::WeakPtr<content::NavigationHandle> handle = Navigate(params_ptr);
  // If navigation started synchronously, run the callback and return the
  // WebContents.
  if (handle) {
    if (navigation_handle_callback) {
      std::move(navigation_handle_callback).Run(*handle);
    }
    return handle->GetWebContents();
  }

  // Else, run asynchronously and return nullptr.
  Navigate(
      params_ptr,
      base::BindOnce(
          [](std::unique_ptr<ChromePopupNavigationDelegate> owned_delegate,
             base::OnceCallback<void(content::NavigationHandle&)> callback,
             base::WeakPtr<content::NavigationHandle> handle) {
            // Check if the handle is valid and dereference it to match the
            // signature expected by OpenURL's callback
            // (NavigationHandle&).
            if (handle && callback) {
              std::move(callback).Run(*handle);
            }
            // If handle is null, callback is never called.
          },
          std::move(popup_delegate), std::move(navigation_handle_callback)));

  return nullptr;
}

base::android::ScopedJavaLocalRef<jobject> AndroidBrowserWindow::GetActivity() {
  return Java_AndroidBrowserWindow_getActivity(AttachCurrentThread(),
                                               java_android_browser_window_);
}

DEFINE_JNI(AndroidBrowserWindow)
