// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_ON_DEVICE_MODEL_UPDATE_LISTENER_H_
#define COMPONENTS_OMNIBOX_BROWSER_ON_DEVICE_MODEL_UPDATE_LISTENER_H_

#include <memory>

#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "base/threading/thread_checker.h"

// This class is used by OnDeviceHeadSuggestComponentInstaller or
// OnDeviceTailModelObserver to hold the filenames for the on device models
// downloaded by corresponding services.
class OnDeviceModelUpdateListener {
 public:

  static OnDeviceModelUpdateListener* GetInstance();

  // Called by Component Updater when head model update is completed to update
  // |head_model_dir_| and |head_model_filename_|.
  void OnHeadModelUpdate(const base::FilePath& model_dir);

  std::string head_model_filename() const;

 private:
  friend class base::NoDestructor<OnDeviceModelUpdateListener>;
  friend class OnDeviceHeadProviderTest;
  friend class OnDeviceModelUpdateListenerTest;

  void ResetListenerForTest();

  OnDeviceModelUpdateListener();
  ~OnDeviceModelUpdateListener();
  OnDeviceModelUpdateListener(const OnDeviceModelUpdateListener&) = delete;
  OnDeviceModelUpdateListener& operator=(const OnDeviceModelUpdateListener&) =
      delete;

  // The directory where the on device head model resides.
  base::FilePath head_model_dir_;

  // The filename of the head model.
  std::string head_model_filename_;

  THREAD_CHECKER(thread_checker_);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_ON_DEVICE_MODEL_UPDATE_LISTENER_H_
