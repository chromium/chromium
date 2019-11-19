// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_ON_DEVICE_MODEL_UPDATE_LISTENER_H_
#define COMPONENTS_OMNIBOX_BROWSER_ON_DEVICE_MODEL_UPDATE_LISTENER_H_

#include <memory>

#include "base/callback.h"
#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "base/threading/thread_checker.h"

// This class is used by OnDeviceHeadSuggestComponentInstaller to notify
// OnDeviceHeadProvider when on device model update is finished.
class OnDeviceModelUpdateListener {
 public:
  using ModelUpdateCallback =
      base::RepeatingCallback<void(const std::string& new_model_filename)>;
  using UpdateCallbacks = base::CallbackList<void(const std::string&)>;
  using UpdateSubscription = UpdateCallbacks::Subscription;

  static OnDeviceModelUpdateListener* GetInstance();

  // Adds a callback which will be run on model update. This method will also
  // notify the provider immediately if a model is available.
  std::unique_ptr<UpdateSubscription> AddModelUpdateCallback(
      ModelUpdateCallback callback);

  // Called by Component Updater when model update is completed to notify the
  // on device head provider to reload the model.
  void OnModelUpdate(const base::FilePath& model_dir);

  std::string model_filename() const { return model_filename_; }

 private:
  friend class base::NoDestructor<OnDeviceModelUpdateListener>;
  friend class OnDeviceHeadProviderTest;

  void ResetListenerForTest();

  OnDeviceModelUpdateListener();
  ~OnDeviceModelUpdateListener();

  // The directory where the on device model resides.
  base::FilePath model_dir_;

  // The filename of the model.
  std::string model_filename_;

  // A list of callbacks which will be run on model update.
  UpdateCallbacks model_update_callbacks_;

  // The task runner which will be used to run file operations and
  // |model_update_callbacks_|.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(OnDeviceModelUpdateListener);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_ON_DEVICE_MODEL_UPDATE_LISTENER_H_
