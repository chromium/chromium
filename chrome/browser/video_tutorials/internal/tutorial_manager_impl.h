// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIDEO_TUTORIALS_INTERNAL_TUTORIAL_MANAGER_IMPL_H_
#define CHROME_BROWSER_VIDEO_TUTORIALS_INTERNAL_TUTORIAL_MANAGER_IMPL_H_

#include "chrome/browser/video_tutorials/internal/tutorial_manager.h"

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/video_tutorials/internal/store.h"

class PrefService;

namespace video_tutorials {

class TutorialManagerImpl : public TutorialManager {
 public:
  using TutorialStore = Store<TutorialGroup>;
  TutorialManagerImpl(std::unique_ptr<TutorialStore> store, PrefService* prefs);
  ~TutorialManagerImpl() override;

 private:
  // TutorialManager implementation.
  void Init(SuccessCallback callback) override;
  void GetTutorials(GetTutorialsCallback callback) override;
  const std::vector<std::string>& GetSupportedLocales() override;
  std::string GetPreferredLocale() override;
  void SetPreferredLocale(const std::string& locale) override;
  void SaveGroups(std::vector<std::unique_ptr<TutorialGroup>> groups,
                  SuccessCallback callback) override;

  void OnInitCompleted(
      SuccessCallback callback,
      bool success,
      std::unique_ptr<std::vector<std::string>> supported_locales);

  void OnTutorialsLoaded(
      GetTutorialsCallback callback,
      bool success,
      std::vector<std::unique_ptr<TutorialGroup>> loaded_group);

  std::unique_ptr<TutorialStore> store_;
  PrefService* prefs_;

  // List of locales for which we have tutorials.
  std::vector<std::string> supported_locales_;

  // We only keep the tutorials for the preferred locale.
  std::unique_ptr<TutorialGroup> tutorial_group_;

  base::WeakPtrFactory<TutorialManagerImpl> weak_ptr_factory_{this};
};

}  // namespace video_tutorials

#endif  // CHROME_BROWSER_VIDEO_TUTORIALS_INTERNAL_TUTORIAL_MANAGER_IMPL_H_
