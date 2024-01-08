// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_TEST_FAKE_INTENT_HELPER_HOST_H_
#define COMPONENTS_ARC_TEST_FAKE_INTENT_HELPER_HOST_H_

#include "ash/components/arc/mojom/intent_helper.mojom.h"
#include "ash/components/arc/session/connection_holder.h"
#include "base/memory/raw_ptr.h"

namespace arc {

// For tests in arc/ that cannot use the real IntentHelperHost implementation
// in //components/arc.
class FakeIntentHelperHost : public mojom::IntentHelperHost {
 public:
  explicit FakeIntentHelperHost(
      ConnectionHolder<arc::mojom::IntentHelperInstance,
                       arc::mojom::IntentHelperHost>* app_connection_holder);
  FakeIntentHelperHost(const FakeIntentHelperHost&) = delete;
  FakeIntentHelperHost& operator=(const FakeIntentHelperHost&) = delete;
  ~FakeIntentHelperHost() override;

  // mojom::IntentHelperHost overrides.
  void OnIconInvalidated(const std::string& package_name) override;
  void OnIntentFiltersUpdated(
      std::vector<IntentFilter> intent_filters) override;
  void OnOpenDownloads() override;
  void OnOpenUrl(const std::string& url) override;
  void OnOpenCustomTab(const std::string& url,
                       int32_t task_id,
                       OnOpenCustomTabCallback callback) override;
  void OnOpenChromePage(mojom::ChromePage page) override;
  void FactoryResetArc() override;
  void OpenWallpaperPicker() override;
  void OpenVolumeControl() override;
  void OnOpenWebApp(const std::string& url) override;
  void LaunchCameraApp(uint32_t intent_id,
                       arc::mojom::CameraIntentMode mode,
                       bool should_handle_result,
                       bool should_down_scale,
                       bool is_secure,
                       int32_t task_id) override;
  void OnIntentFiltersUpdatedForPackage(
      const std::string& package_name,
      std::vector<IntentFilter> intent_filters) override;
  void CloseCameraApp() override;
  void IsChromeAppEnabled(arc::mojom::ChromeApp app,
                          IsChromeAppEnabledCallback callback) override;
  void OnSupportedLinksChanged(
      std::vector<arc::mojom::SupportedLinksPackagePtr> added_packages,
      std::vector<arc::mojom::SupportedLinksPackagePtr> removed_packages,
      arc::mojom::SupportedLinkChangeSource source) override;
  void OnDownloadAddedDeprecated(
      const std::string& relative_path,
      const std::string& owner_package_name) override;
  void OnOpenAppWithIntent(const GURL& start_url,
                           arc::mojom::LaunchIntentPtr intent) override;
  void OnOpenGlobalActions() override;
  void OnCloseSystemDialogs() override;
  void OnAndroidSettingChange(arc::mojom::AndroidSetting setting,
                              bool is_enabled) override;

 private:
  // The connection holder must outlive |this| object.
  const raw_ptr<ConnectionHolder<arc::mojom::IntentHelperInstance,
                                 arc::mojom::IntentHelperHost>>
      intent_helper_connection_holder_;
};

}  // namespace arc

#endif  // COMPONENTS_ARC_TEST_FAKE_INTENT_HELPER_HOST_H_
