// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_ARC_ARC_APP_SCOPED_PREF_UPDATE_H_
#define CHROME_BROWSER_UI_APP_LIST_ARC_ARC_APP_SCOPED_PREF_UPDATE_H_

#include <string>

#include "components/prefs/scoped_user_pref_update.h"

class PrefService;

namespace arc {

// Pref updater for ARC apps. Used in deferent pref sections.
class ArcAppScopedPrefUpdate : public DictionaryPrefUpdate {
 public:
  // This is used in following cases:
  // |path| is "arc.apps" - To update ARC apps preferences. In this case |id|
  //     defines app id.
  // |path| is "arc.apps.default" - To update ARC default apps preferences. In
  //     this case |id| defines app id.
  // |path| is "arc.packages" - To update ARC packages preferences. In this case
  //    |id| is package name.
  ArcAppScopedPrefUpdate(PrefService* service,
                         const std::string& id,
                         const std::string& path);

  ArcAppScopedPrefUpdate(const ArcAppScopedPrefUpdate&) = delete;
  ArcAppScopedPrefUpdate& operator=(const ArcAppScopedPrefUpdate&) = delete;

  ~ArcAppScopedPrefUpdate() override;

  // DictionaryPrefUpdate:
  base::Value* Get() override;

 private:
  const std::string id_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_UI_APP_LIST_ARC_ARC_APP_SCOPED_PREF_UPDATE_H_
