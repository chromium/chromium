// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/webui/webui_bundled_code_cache_map.h"

#include <optional>

#include "ui/webui/buildflags.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_WEBUI_GENERATE_CODE_CACHE)
#include <string_view>
#include <utility>

#include "base/containers/fixed_flat_map.h"
#include "base/containers/map_util.h"
#include "base/containers/span.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/side_panel_bookmarks_code_cache_resources_map.h"
#include "chrome/grit/side_panel_customize_chrome_code_cache_resources_map.h"
#include "chrome/grit/side_panel_reading_list_code_cache_resources_map.h"
#include "chrome/grit/side_panel_shared_code_cache_resources_map.h"
#include "chrome/grit/side_panel_tabs_from_other_devices_code_cache_resources_map.h"
#include "chrome/grit/webui_toolbar_code_cache_resources_map.h"
#include "content/public/common/url_constants.h"
#include "ui/base/webui/resource_path.h"
#include "ui/webui/resources/grit/webui_code_cache_resources_map.h"

#if BUILDFLAG(ENABLE_WEBUI_NTP)
#include "base/feature_list.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/new_tab_page_code_cache_resources_map.h"
#include "chrome/grit/new_tab_page_eager_code_cache_resources_map.h"
#include "components/search/ntp_features.h"
#endif  // BUILDFLAG(ENABLE_WEBUI_NTP)
#endif  // BUILDFLAG(ENABLE_WEBUI_GENERATE_CODE_CACHE)

namespace webui {

#if BUILDFLAG(ENABLE_WEBUI_GENERATE_CODE_CACHE)
namespace {

constexpr std::string_view kCodeCacheSuffix = ".code_cache";

// Linearly scans `resources` because GRIT's generated `ResourcePath` arrays are
// unsorted. Each lookup is scoped to a single host's bundle (typically a
// handful of bundled chunks) and checks length before comparing paths, avoiding
// per-renderer heap allocations or runtime map construction.
std::optional<int> FindResourceId(base::span<const ResourcePath> resources,
                                  std::string_view resource_path) {
  const size_t expected_length = resource_path.size() + kCodeCacheSuffix.size();
  for (const ResourcePath& entry : resources) {
    std::string_view entry_path(entry.path);
    if (entry_path.size() == expected_length &&
        entry_path.starts_with(resource_path) &&
        entry_path.ends_with(kCodeCacheSuffix)) {
      return entry.id;
    }
  }
  return std::nullopt;
}

}  // namespace
#endif  // BUILDFLAG(ENABLE_WEBUI_GENERATE_CODE_CACHE)

std::optional<int> GetWebUIBundledCodeCacheResourceId(
    [[maybe_unused]] const GURL& webui_resource_url) {
#if BUILDFLAG(ENABLE_WEBUI_GENERATE_CODE_CACHE)
  if (!webui_resource_url.is_valid()) {
    return std::nullopt;
  }

  // Reject root URLs with no resource path (e.g. "chrome://resources/", where
  // `path` is "/") and non-hierarchical URLs before stripping the leading '/'.
  std::string_view path = webui_resource_url.path();
  if (!path.starts_with('/') || path.size() <= 1) {
    return std::nullopt;
  }
  path.remove_prefix(1);

  const std::string_view scheme = webui_resource_url.scheme();
  const std::string_view host = webui_resource_url.host();
  if (host == content::kChromeUIResourcesHost &&
      (scheme == content::kChromeUIScheme ||
       scheme == content::kChromeUIUntrustedScheme)) {
    return FindResourceId(kWebuiCodeCacheResources, path);
  }

  if (scheme != content::kChromeUIScheme) {
    return std::nullopt;
  }

  using ResourcePathSpan = base::span<const ResourcePath>;
  // `{host_bundle, optional_shared_bundle}`. Using `std::pair` rather than a
  // custom struct avoids `raw_span` non-trivial destructors in `static
  // constexpr`.
  using HostBundles = std::pair<ResourcePathSpan, ResourcePathSpan>;

  static constexpr auto kHostResources =
      base::MakeFixedFlatMap<std::string_view, HostBundles>({
          {chrome::kChromeUIBookmarksSidePanelHost,
           {kSidePanelBookmarksCodeCacheResources,
            kSidePanelSharedCodeCacheResources}},
          {chrome::kChromeUICustomizeChromeSidePanelHost,
           {kSidePanelCustomizeChromeCodeCacheResources,
            kSidePanelSharedCodeCacheResources}},
          {chrome::kChromeUIReadLaterHost,
           {kSidePanelReadingListCodeCacheResources,
            kSidePanelSharedCodeCacheResources}},
          {chrome::kChromeUITabsFromOtherDevicesSidePanelHost,
           {kSidePanelTabsFromOtherDevicesCodeCacheResources,
            kSidePanelSharedCodeCacheResources}},
          {chrome::kChromeUIWebUIToolbarHost,
           {kWebuiToolbarCodeCacheResources, {}}},
      });
  if (const HostBundles* bundles = base::FindOrNull(kHostResources, host)) {
    const auto& [host_bundle, shared_bundle] = *bundles;
    if (!shared_bundle.empty() && path.starts_with("shared/")) {
      if (std::optional<int> id = FindResourceId(shared_bundle, path)) {
        return id;
      }
    }
    return FindResourceId(host_bundle, path);
  }

#if BUILDFLAG(ENABLE_WEBUI_NTP)
  // Must mirror the lazy/eager bundle selection in `NewTabPageUI`; V8 rejects
  // a cache built from the other bundle.
  // TODO(crbug.com/576324502): Move into `kHostResources` once
  // `kNewTabPageEagerCodeCacheResources` is removed.
  if (host == chrome::kChromeUINewTabPageHost &&
      base::FeatureList::IsEnabled(
          features::kWebUIBundledCodeCacheForNewTabPage)) {
    const ResourcePathSpan resources =
        base::FeatureList::IsEnabled(ntp_features::kNtpLazyLoadComposebox)
            ? ResourcePathSpan(kNewTabPageCodeCacheResources)
            : ResourcePathSpan(kNewTabPageEagerCodeCacheResources);
    return FindResourceId(resources, path);
  }
#endif  // BUILDFLAG(ENABLE_WEBUI_NTP)
#endif  // BUILDFLAG(ENABLE_WEBUI_GENERATE_CODE_CACHE)
  return std::nullopt;
}

}  // namespace webui
