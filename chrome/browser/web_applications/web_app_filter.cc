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
  return IsTrue(SimpleCondition::kOpensInDedicatedWindow);
}

// static
WebAppFilter WebAppFilter::IsIsolatedApp() {
  return IsTrue(SimpleCondition::kIsolatedApp);
}

// static
WebAppFilter WebAppFilter::IsDevModeIsolatedApp() {
  return IsTrue(SimpleCondition::kIsolatedAppDevMode);
}

// static
WebAppFilter WebAppFilter::IsIsolatedSubApp() {
  return IsTrue(SimpleCondition::kIsolatedSubApp);
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
  return !IsTrue(SimpleCondition::kIsDiy);
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
         IsTrue(SimpleCondition::kIsDiy);
}

// static
WebAppFilter WebAppFilter::LaunchableFromInstallApi() {
  return IsTrue(SimpleCondition::kWasInstalledByUser) |
         OpensInDedicatedWindow();
}

// static
WebAppFilter WebAppFilter::IsTrusted() {
  return IsTrue(SimpleCondition::kInstalledByTrustedSource);
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
  leaf.predicate =
      ManagementRequirement{ManagementRequirement::Type::kHasAny, {source}};
  return WebAppFilter(std::move(leaf));
}

// static
WebAppFilter WebAppFilter::HasAnySource(WebAppManagementTypes sources) {
  LeafFilter leaf;
  leaf.predicate =
      ManagementRequirement{ManagementRequirement::Type::kHasAny, sources};
  return WebAppFilter(std::move(leaf));
}

// static
WebAppFilter WebAppFilter::HasAllSources(WebAppManagementTypes sources) {
  LeafFilter leaf;
  leaf.predicate =
      ManagementRequirement{ManagementRequirement::Type::kHasAll, sources};
  return WebAppFilter(std::move(leaf));
}

// static
WebAppFilter WebAppFilter::InstallStateIs(proto::InstallState state) {
  LeafFilter leaf;
  leaf.predicate = InstallStateSet{state};
  return WebAppFilter(std::move(leaf));
}

// static
WebAppFilter WebAppFilter::InstallStateIsAnyOf(InstallStateSet states) {
  LeafFilter leaf;
  leaf.predicate = states;
  return WebAppFilter(std::move(leaf));
}

// static
WebAppFilter WebAppFilter::IsTrue(SimpleCondition condition) {
  LeafFilter leaf;
  leaf.predicate = condition;
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
