// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/pinned_tab_codec.h"

#include <stddef.h>

#include <utility>

#include "base/values.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension.h"

using content::NavigationEntry;

// Key used in dictionaries for the url.
static const char kURL[] = "url";

// Returns true if |browser| has any pinned tabs.
static bool HasPinnedTabs(Browser* browser) {
  TabStripModel* tab_model = browser->tab_strip_model();
  for (int i = 0; i < tab_model->count(); ++i) {
    if (tab_model->IsTabPinned(i))
      return true;
  }
  return false;
}

// Adds a DictionaryValue to |values| representing |tab|.
static void EncodeTab(const StartupTab& tab, base::ListValue* values) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue);
  value->SetString(kURL, tab.url.spec());
  values->Append(std::move(value));
}

// Adds a base::DictionaryValue to |values| representing the pinned tab at the
// specified index.
static void EncodePinnedTab(TabStripModel* model,
                            int index,
                            base::ListValue* values) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());

  content::WebContents* web_contents = model->GetWebContentsAt(index);
  NavigationEntry* entry =
      web_contents->GetController().GetLastCommittedEntry();
  if (entry) {
    value->SetString(kURL, entry->GetURL().spec());
    values->Append(std::move(value));
  }
}

// Invokes EncodePinnedTab for each pinned tab in browser.
static void EncodePinnedTabs(Browser* browser, base::ListValue* values) {
  TabStripModel* tab_model = browser->tab_strip_model();
  for (int i = 0; i < tab_model->count() && tab_model->IsTabPinned(i); ++i)
    EncodePinnedTab(tab_model, i, values);
}

// Decodes the previously written values in |value| to |tab|, returning true
// on success.
static bool DecodeTab(const base::DictionaryValue& value, StartupTab* tab) {
  std::string url_string;
  if (!value.GetString(kURL, &url_string))
    return false;
  tab->url = GURL(url_string);

  return true;
}

// static
void PinnedTabCodec::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kPinnedTabs);
}

// static
void PinnedTabCodec::WritePinnedTabs(Profile* profile) {
  PrefService* prefs = profile->GetPrefs();
  if (!prefs)
    return;

  base::ListValue values;
  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser->is_type_normal() && browser->profile() == profile &&
        HasPinnedTabs(browser)) {
      EncodePinnedTabs(browser, &values);
    }
  }
  prefs->Set(prefs::kPinnedTabs, values);
}

// static
void PinnedTabCodec::WritePinnedTabs(Profile* profile,
                                     const StartupTabs& tabs) {
  PrefService* prefs = profile->GetPrefs();
  if (!prefs)
    return;

  ListPrefUpdate update(prefs, prefs::kPinnedTabs);
  base::ListValue* values = update.Get();
  values->Clear();
  for (auto i = tabs.begin(); i != tabs.end(); ++i)
    EncodeTab(*i, values);
}

// static
StartupTabs PinnedTabCodec::ReadPinnedTabs(Profile* profile) {
  PrefService* prefs = profile->GetPrefs();
  if (!prefs)
    return StartupTabs();
  return ReadPinnedTabs(prefs->GetList(prefs::kPinnedTabs));
}

// static
StartupTabs PinnedTabCodec::ReadPinnedTabs(const base::Value* value) {
  StartupTabs results;

  const base::ListValue* tabs_list = NULL;
  if (!value->GetAsList(&tabs_list))
    return results;

  for (size_t i = 0, max = tabs_list->GetSize(); i < max; ++i) {
    const base::DictionaryValue* tab_values = NULL;
    if (tabs_list->GetDictionary(i, &tab_values)) {
      StartupTab tab;
      if (DecodeTab(*tab_values, &tab))
        results.push_back(tab);
    }
  }
  return results;
}
