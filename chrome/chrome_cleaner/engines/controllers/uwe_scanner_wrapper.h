// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_ENGINES_CONTROLLERS_UWE_SCANNER_WRAPPER_H_
#define CHROME_CHROME_CLEANER_ENGINES_CONTROLLERS_UWE_SCANNER_WRAPPER_H_

#include <memory>
#include <vector>

#include "chrome/chrome_cleaner/chrome_utils/force_installed_extension.h"
#include "chrome/chrome_cleaner/constants/uws_id.h"
#include "chrome/chrome_cleaner/proto/uwe_matcher.pb.h"
#include "chrome/chrome_cleaner/scanner/scanner.h"

namespace chrome_cleaner {

// Scans for Unwanted Extensions whenever a UwS is found.
class UwEScannerWrapper : public Scanner {
 public:
  // |scanner| is the scanner that this scanner will wrap.
  // It will drive all the interactions with the only change being that the
  // found uws callback will trigger a search for UwE whenever a UwS is found.
  // The |matchers| will match the UwS with the UwE from the
  // |force_installed_extensions| list
  UwEScannerWrapper(
      std::unique_ptr<Scanner> scanner,
      UwEMatchers* matchers,
      const std::vector<ForceInstalledExtension>& force_installed_extensions);
  UwEScannerWrapper(UwEScannerWrapper&& wrapper);
  UwEScannerWrapper& operator=(UwEScannerWrapper&& other);
  ~UwEScannerWrapper() override;

  // Scanner implementation.

  // Will simply call the wrapped scanner's start method.
  bool Start(const FoundUwSCallback& found_uws_callback,
             DoneCallback done_callback) override;

  void Stop() override;

  bool IsCompletelyDone() const override;

 private:
  // Finds the UwE associated with the UwS and stores in in the PUP data.
  void FindUwE(UwSId found_uws);

  std::unique_ptr<Scanner> scanner_;

  UwEMatchers* matchers_;

  std::vector<ForceInstalledExtension> force_installed_extensions_;

  FoundUwSCallback found_uws_callback_;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_ENGINES_CONTROLLERS_UWE_SCANNER_WRAPPER_H_
