// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LACROS_NATIVE_THEME_CACHE_H_
#define CHROMEOS_LACROS_NATIVE_THEME_CACHE_H_

#include "chromeos/crosapi/mojom/native_theme.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chromeos {

// This instance connects to ash-chrome, listens to native theme info changes,
// and caches the info for later synchronous reads using getters.
class COMPONENT_EXPORT(CHROMEOS_LACROS) NativeThemeCache
    : public crosapi::mojom::NativeThemeInfoObserver {
 public:
  explicit NativeThemeCache(const crosapi::mojom::NativeThemeInfo& info);

  NativeThemeCache(const NativeThemeCache&) = delete;
  NativeThemeCache& operator=(const NativeThemeCache&) = delete;
  ~NativeThemeCache() override;

  // Start observing native theme info changes in ash-chrome.
  // This is a post-construction step to decouple from LacrosService.
  void Start();

 private:
  // crosapi::mojom::NativeThemeInfoObserver:
  void OnNativeThemeInfoChanged(
      crosapi::mojom::NativeThemeInfoPtr info) override;

  void SetNativeThemeInfo();

  // Cached native theme info.
  crosapi::mojom::NativeThemeInfoPtr info_;

  // Receives mojo messages from ash-chromem (under Streaming mode).
  mojo::Receiver<crosapi::mojom::NativeThemeInfoObserver> receiver_{this};
};

}  // namespace chromeos

#endif  // CHROMEOS_LACROS_NATIVE_THEME_CACHE_H_
