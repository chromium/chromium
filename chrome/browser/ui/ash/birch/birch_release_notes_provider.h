// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_BIRCH_BIRCH_RELEASE_NOTES_PROVIDER_H_
#define CHROME_BROWSER_UI_ASH_BIRCH_BIRCH_RELEASE_NOTES_PROVIDER_H_

#include <optional>
#include "ash/birch/birch_data_provider.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/release_notes/release_notes_storage.h"
#include "chrome/browser/profiles/profile.h"

namespace ash {

class ASH_EXPORT BirchReleaseNotesProvider : public BirchDataProvider {
 public:
  explicit BirchReleaseNotesProvider(Profile* profile);
  BirchReleaseNotesProvider(const BirchReleaseNotesProvider&) = delete;
  BirchReleaseNotesProvider& operator=(const BirchReleaseNotesProvider&) =
      delete;
  ~BirchReleaseNotesProvider() override;

  // BirchDataProvider
  void RequestBirchDataFetch() override;

 private:
  bool IsTimeframeToShowBirchEnded();

  const raw_ptr<Profile> profile_;
  ash::ReleaseNotesStorage release_notes_storage_;
  std::optional<base::Time> first_seen_time_;
  base::WeakPtrFactory<BirchReleaseNotesProvider> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_BIRCH_BIRCH_RELEASE_NOTES_PROVIDER_H_
