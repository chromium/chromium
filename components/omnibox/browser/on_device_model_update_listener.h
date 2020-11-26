// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_ON_DEVICE_MODEL_UPDATE_LISTENER_H_
#define COMPONENTS_OMNIBOX_BROWSER_ON_DEVICE_MODEL_UPDATE_LISTENER_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "base/threading/thread_checker.h"

// This class is used by OnDeviceHeadSuggestComponentInstaller to hold the
// directory & filename for the on device model downloaded by Component Updater.
class OnDeviceModelUpdateListener {
 public:

  static OnDeviceModelUpdateListener* GetInstance();

  // Called by Component Updater when model update is completed to update
  // |model_dir_| and |model_filename_|.
  void OnModelUpdate(const base::FilePath& model_dir);

  std::string model_filename() const;

 private:
  friend class base::NoDestructor<OnDeviceModelUpdateListener>;
  friend class OnDeviceHeadProviderTest;

  void ResetListenerForTest();

  OnDeviceModelUpdateListener();
  ~OnDeviceModelUpdateListener();
  OnDeviceModelUpdateListener(const OnDeviceModelUpdateListener&) = delete;
  OnDeviceModelUpdateListener& operator=(const OnDeviceModelUpdateListener&) =
      delete;

  // The directory where the on device model resides.
  base::FilePath model_dir_;

  // The filename of the model.
  std::string model_filename_;

  THREAD_CHECKER(thread_checker_);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_ON_DEVICE_MODEL_UPDATE_LISTENER_H_
