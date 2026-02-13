// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_filter.h"

#include <utility>

#include "base/check_is_test.h"

namespace web_app {

// static
WebAppFilter WebAppFilter::OpensInBrowserTab() {
  LeafFilter leaf;
  leaf.opens_in_browser_tab = true;
  return WebAppFilter(std::move(leaf));
}

// static
WebAppFilter WebAppFilter::OpensInDedicatedWindow() {
  LeafFilter leaf;
  leaf.opens_in_dedicated_window = true;
  return WebAppFilter(std::move(leaf));
}

// static
WebAppFilter WebAppFilter::IsIsolatedApp() {
  LeafFilter leaf;
  leaf.isolated_app_filter = IsolatedWebAppFilter();
  return WebAppFilter(std::move(leaf));
}

// static
WebAppFilter WebAppFilter::IsDevModeIsolatedApp() {
  LeafFilter leaf;
  leaf.isolated_app_filter = IsolatedWebAppFilter{.must_be_in_dev_mode = true};
  return WebAppFilter(std::move(leaf));
}

// static
WebAppFilter WebAppFilter::IsIsolatedSubApp() {
  LeafFilter leaf;
  leaf.isolated_app_filter = IsolatedWebAppFilter{.is_sub_app = true};
  return WebAppFilter(std::move(leaf));
}

// static
WebAppFilter WebAppFilter::PolicyInstalledIsolatedWebApp() {
  LeafFilter leaf;
  leaf.isolated_app_filter =
      IsolatedWebAppFilter{.must_be_policy_installed = true};
  return WebAppFilter(std::move(leaf));
}

// static
WebAppFilter WebAppFilter::UserInstalledIsolatedWebApp() {
  LeafFilter leaf;
  leaf.isolated_app_filter =
      IsolatedWebAppFilter{.must_be_user_installed = true};
  return WebAppFilter(std::move(leaf));
}

// static
WebAppFilter WebAppFilter::IsIsolatedWebAppWithOnlyUserManagement() {
  LeafFilter leaf;
  leaf.isolated_app_filter =
      IsolatedWebAppFilter{.must_have_no_external_management = true};
  return WebAppFilter(std::move(leaf));
}

// static
WebAppFilter WebAppFilter::IsCraftedApp() {
  LeafFilter leaf;
  leaf.is_crafted_app = true;
  return WebAppFilter(std::move(leaf));
}

// static
WebAppFilter WebAppFilter::IsCraftedAppAndOpensInDedicatedWindow() {
  LeafFilter leaf;
  leaf.is_crafted_app_and_opens_in_dedicated_window = true;
  return WebAppFilter(std::move(leaf));
}

// static
WebAppFilter WebAppFilter::IsSuggestedApp() {
  LeafFilter leaf;
  leaf.is_suggested_app = true;
  return WebAppFilter(std::move(leaf));
}

// static
WebAppFilter WebAppFilter::DisplaysBadgeOnOs() {
  LeafFilter leaf;
  leaf.displays_badge_on_os = true;
  return WebAppFilter(std::move(leaf));
}

// static
WebAppFilter WebAppFilter::SupportsOsNotifications() {
  LeafFilter leaf;
  leaf.supports_os_notifications = true;
  return WebAppFilter(std::move(leaf));
}

// static
WebAppFilter WebAppFilter::InstalledInChrome() {
  LeafFilter leaf;
  leaf.installed_in_chrome = true;
  return WebAppFilter(std::move(leaf));
}

// static
WebAppFilter WebAppFilter::InstalledInOperatingSystemForTesting() {
  CHECK_IS_TEST();
  LeafFilter leaf;
  leaf.installed_in_os = true;
  return WebAppFilter(std::move(leaf));
}

// static
WebAppFilter WebAppFilter::IsDiyWithOsShortcut() {
  LeafFilter leaf;
  leaf.is_diy_with_os_shortcut = true;
  return WebAppFilter(std::move(leaf));
}

// static
WebAppFilter WebAppFilter::LaunchableFromInstallApi() {
  LeafFilter leaf;
  leaf.launchable_from_install_api = true;
  return WebAppFilter(std::move(leaf));
}

// static
WebAppFilter WebAppFilter::IsTrusted() {
  LeafFilter leaf;
  leaf.is_app_trusted = true;
  return WebAppFilter(std::move(leaf));
}

// static
WebAppFilter WebAppFilter::IsIsolatedWebAppIncludingUninstalling() {
  LeafFilter leaf;
  leaf.is_isolated_apps_including_uninstalling = true;
  return WebAppFilter(std::move(leaf));
}

// static
WebAppFilter WebAppFilter::IsAppSuggestedForMigration() {
  LeafFilter leaf;
  leaf.is_app_suggested_from_migration = true;
  return WebAppFilter(std::move(leaf));
}

// static
WebAppFilter WebAppFilter::IsAppSurfaceableToUser() {
  LeafFilter leaf;
  leaf.is_app_surfaceable_to_user = true;
  return WebAppFilter(std::move(leaf));
}

// static
WebAppFilter WebAppFilter::IsAppValidMigrationSource() {
  LeafFilter leaf;
  leaf.is_valid_migration_source = true;
  return WebAppFilter(std::move(leaf));
}

// static
WebAppFilter WebAppFilter::IsAppEligibleForManifestUpdate() {
  LeafFilter leaf;
  leaf.is_app_eligible_for_manifest_update = true;
  return WebAppFilter(std::move(leaf));
}

WebAppFilter::LeafFilter::LeafFilter() = default;
WebAppFilter::LeafFilter::~LeafFilter() = default;
WebAppFilter::LeafFilter::LeafFilter(const LeafFilter&) = default;
WebAppFilter::LeafFilter::LeafFilter(LeafFilter&&) noexcept = default;
WebAppFilter::LeafFilter& WebAppFilter::LeafFilter::operator=(
    LeafFilter&&) noexcept = default;

WebAppFilter::WebAppFilter(LeafFilter leaf) : data_(std::move(leaf)) {}

WebAppFilter::WebAppFilter(WebAppFilter left,
                           WebAppFilter right,
                           BinaryOp::Op op)
    : data_(BinaryOp(std::make_unique<WebAppFilter>(std::move(left)),
                     std::make_unique<WebAppFilter>(std::move(right)),
                     op)) {}

WebAppFilter::BinaryOp::BinaryOp(std::unique_ptr<WebAppFilter> left,
                                 std::unique_ptr<WebAppFilter> right,
                                 Op op)
    : left(std::move(left)), right(std::move(right)), op(op) {}

WebAppFilter::BinaryOp::~BinaryOp() = default;

WebAppFilter::BinaryOp::BinaryOp(const BinaryOp& other)
    : left(std::make_unique<WebAppFilter>(*other.left)),
      right(std::make_unique<WebAppFilter>(*other.right)),
      op(other.op) {}

WebAppFilter::BinaryOp::BinaryOp(BinaryOp&&) noexcept = default;

WebAppFilter::BinaryOp& WebAppFilter::BinaryOp::operator=(BinaryOp&&) noexcept =
    default;

WebAppFilter::WebAppFilter(const WebAppFilter&) = default;
WebAppFilter::WebAppFilter(WebAppFilter&&) noexcept = default;
WebAppFilter& WebAppFilter::operator=(WebAppFilter&&) noexcept = default;
WebAppFilter::~WebAppFilter() = default;

WebAppFilter operator&(WebAppFilter lhs, WebAppFilter rhs) {
  return WebAppFilter(std::move(lhs), std::move(rhs),
                      WebAppFilter::BinaryOp::Op::kAnd);
}

WebAppFilter operator|(WebAppFilter lhs, WebAppFilter rhs) {
  return WebAppFilter(std::move(lhs), std::move(rhs),
                      WebAppFilter::BinaryOp::Op::kOr);
}

WebAppFilter operator!(WebAppFilter filter) {
  return WebAppFilter(WebAppFilter::IsAppEligibleForManifestUpdate(),
                      std::move(filter), WebAppFilter::BinaryOp::Op::kExclude);
}

}  // namespace web_app
