// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_TAB_MATCHER_H_
#define COMPONENTS_OMNIBOX_BROWSER_TAB_MATCHER_H_

#include <functional>
#include <unordered_map>

#include "build/build_config.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_weak_ref.h"
#endif

// Abstraction of a mechanism that associates GURL objects with open tabs.
class TabMatcher {
 public:
  // Information about presence of open tabs that match supplied URL.
  // The open tab information may be platform specific.
  struct TabInfo {
    // Whether a tab with matching URL exists.
    bool has_matching_tab{};

#if BUILDFLAG(IS_ANDROID)
    // Weak pointer to an Android Tab for the supplied GURL.
    JavaObjectWeakGlobalRef android_tab{};
#endif
  };

  // Mechanism that facilitates hashing of the GURL objects.
  struct GURLHash {
    size_t operator()(const GURL& url) const {
      return std::hash<std::string>()(url.spec());
    }
  };

  // Wrapper for tab information used by OpenTabProvider.
  struct TabWrapper {
    std::u16string title;
    GURL url;

    TabWrapper(std::u16string title, GURL url) {
      this->title = title;
      this->url = url;
    }
  };

  // Map of URLs to TabInfo used for batch tab lookups.
  // Note this uses ptr_hash<> for lookups: objects used for insertion must
  // outlive the map and serve as direct keys.
  using GURLToTabInfoMap = std::unordered_map<GURL, TabInfo, GURLHash>;

  TabMatcher() = default;
  TabMatcher(TabMatcher&&) = delete;
  TabMatcher(const TabMatcher&) = delete;
  TabMatcher& operator=(TabMatcher&&) = delete;
  TabMatcher& operator=(const TabMatcher&) = delete;

  virtual ~TabMatcher() = default;

  // For a given URL, check if a tab already exists where that URL is already
  // opened.
  // Returns true, if the URL can be matched to existing tab, otherwise false.
  virtual bool IsTabOpenWithURL(const GURL& gurl,
                                const AutocompleteInput* input) const = 0;

  // For a given input GURLToTabInfoMap, in-place update the map with the
  // TabInfo details.
  // The matching operation is performed in a batch, offering performance
  // benefits on Android where the operation is otherwise very expensive.
  virtual void FindMatchingTabs(GURLToTabInfoMap* map,
                                const AutocompleteInput* input) const;

  // Returns tab wrappers for all open tabs for the current profile.
  virtual std::vector<TabWrapper> GetOpenTabs() const;
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_TAB_MATCHER_H_
