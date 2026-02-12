// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_filter.h"

#include <utility>
#include <variant>

#include "base/check_is_test.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/web_app_management_type.h"

namespace web_app {

// static
WebAppFilter WebAppFilter::OpensInBrowserTab() {
  return !OpensInDedicatedWindow();
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
  return IsIsolatedApp() & HasSource(WebAppManagement::kIwaPolicy);
}

// static
WebAppFilter WebAppFilter::UserInstalledIsolatedWebApp() {
  return IsIsolatedApp() & HasSource(WebAppManagement::kIwaUserInstalled);
}

// static
WebAppFilter WebAppFilter::IsIsolatedWebAppWithOnlyUserManagement() {
  return IsIsolatedApp() & !HasAnySource({WebAppManagement::kKiosk,
                                          WebAppManagement::kIwaShimlessRma,
                                          WebAppManagement::kIwaPolicy});
}

// static
WebAppFilter WebAppFilter::IsCraftedApp() {
  LeafFilter leaf;
  leaf.is_crafted_app = true;
  return WebAppFilter(std::move(leaf));
}

// static
WebAppFilter WebAppFilter::IsSuggestedApp() {
  return InstallStateIs(proto::InstallState::SUGGESTED_FROM_ANOTHER_DEVICE);
}

// static
WebAppFilter WebAppFilter::DisplaysBadgeOnOs() {
  return InstallStateIs(proto::InstallState::INSTALLED_WITH_OS_INTEGRATION);
}

// static
WebAppFilter WebAppFilter::SupportsOsNotifications() {
  return InstallStateIs(proto::InstallState::INSTALLED_WITH_OS_INTEGRATION);
}

// static
WebAppFilter WebAppFilter::InstalledInChrome() {
  return InstallStateIsAnyOf(
      {proto::InstallState::INSTALLED_WITH_OS_INTEGRATION,
       proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION});
}

// static
WebAppFilter WebAppFilter::InstalledInOperatingSystemForTesting() {
  CHECK_IS_TEST();
  return InstallStateIs(proto::InstallState::INSTALLED_WITH_OS_INTEGRATION);
}

// static
WebAppFilter WebAppFilter::IsDiyWithOsShortcut() {
  return InstallStateIs(proto::InstallState::INSTALLED_WITH_OS_INTEGRATION) &
         !IsCraftedApp();
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
  return InstallStateIs(proto::InstallState::SUGGESTED_FROM_MIGRATION);
}

// static
WebAppFilter WebAppFilter::IsAppSurfaceableToUser() {
  return !InstallStateIs(proto::InstallState::SUGGESTED_FROM_MIGRATION);
}

// static
WebAppFilter WebAppFilter::IsAppValidMigrationSource() {
  return InstallStateIs(proto::InstallState::INSTALLED_WITH_OS_INTEGRATION) &
         !HasSource(WebAppManagement::Type::kPolicy) & !IsIsolatedApp();
}

// static
WebAppFilter WebAppFilter::IsAppEligibleForManifestUpdate() {
  return IsInRegistrar();
}

// static
WebAppFilter WebAppFilter::HasSource(WebAppManagement::Type source) {
  LeafFilter leaf;
  leaf.management_requirement =
      ManagementRequirement{ManagementRequirement::Type::kHasAny, {source}};
  return WebAppFilter(std::move(leaf));
}

// static
WebAppFilter WebAppFilter::HasAnySource(WebAppManagementTypes sources) {
  LeafFilter leaf;
  leaf.management_requirement =
      ManagementRequirement{ManagementRequirement::Type::kHasAny, sources};
  return WebAppFilter(std::move(leaf));
}

// static
WebAppFilter WebAppFilter::HasAllSources(WebAppManagementTypes sources) {
  LeafFilter leaf;
  leaf.management_requirement =
      ManagementRequirement{ManagementRequirement::Type::kHasAll, sources};
  return WebAppFilter(std::move(leaf));
}

// static
WebAppFilter WebAppFilter::InstallStateIs(proto::InstallState state) {
  LeafFilter leaf;
  leaf.install_state_requirement = {{state}};
  return WebAppFilter(std::move(leaf));
}

// static
WebAppFilter WebAppFilter::InstallStateIsAnyOf(InstallStateSet states) {
  LeafFilter leaf;
  leaf.install_state_requirement = states;
  return WebAppFilter(std::move(leaf));
}

// static
WebAppFilter WebAppFilter::IsInRegistrar() {
  return WebAppFilter::InstallStateIsAnyOf(
      {proto::InstallState::INSTALLED_WITH_OS_INTEGRATION,
       proto::InstallState::INSTALLED_WITHOUT_OS_INTEGRATION,
       proto::InstallState::SUGGESTED_FROM_ANOTHER_DEVICE,
       proto::InstallState::SUGGESTED_FROM_MIGRATION});
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
  return WebAppFilter(WebAppFilter::IsInRegistrar(), std::move(filter),
                      WebAppFilter::BinaryOp::Op::kExclude);
}

}  // namespace web_app
