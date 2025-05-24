// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_DETECTION_CORE_BROWSER_LANGUAGE_DETECTION_MODEL_PROVIDER_H_
#define COMPONENTS_LANGUAGE_DETECTION_CORE_BROWSER_LANGUAGE_DETECTION_MODEL_PROVIDER_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/language_detection/core/background_file.h"

namespace base {
class SequencedTaskRunner;
}
namespace language_detection {

// The maximum number of pending model requests allowed to be kept
// by the LanguageDetectionModelProvider.
inline constexpr int kMaxPendingRequestsAllowed = 100;

// Manages access to a file containing the language detection model. It keeps
// that file open and provides new (copied) instances of `base::File` for that
// file on request.
class LanguageDetectionModelProvider {
 public:
  using GetModelCallback = base::OnceCallback<void(base::File)>;

  // `background_task_runner` is a sequence used for blocking IO operations.
  explicit LanguageDetectionModelProvider(
      const scoped_refptr<base::SequencedTaskRunner>& background_task_runner);
  ~LanguageDetectionModelProvider();

  // Provides the language detection model file. It will asynchronously call
  // `callback` with the file when availability is known. The callback is always
  // asynchronous, even if the model is already available. If the model is
  // definitively unavailable or if too many calls to this are
  // pending, the provided file will be the invalid `base::File()`.
  void GetLanguageDetectionModelFile(GetModelCallback callback);

  // Provide a path to a new model file that should be used going forward. This
  // performs blocking IO operations on the `background_task_runner`.
  void ReplaceModelFile(base::FilePath path);

  // Unloads the model in a background task. This does not set
  // `has_model_ever_been_set_ = false`. After this any requests for the model
  // file will immediately receive an invalid file, until an update with a
  // valid file occurs.
  void UnloadModelFile();

  // Returns whether a valid model is available. The method will return false if
  // `has_model_ever_been_set_ == false` or the model file is invalid.
  bool HasValidModelFile();

 private:
  // Replaces the current model file with a new one. It is careful to
  // open/close files as necessary on a background thread.
  void UpdateModelFile(base::File model_file);

  // Called after the model file changes. It records the fact that the model
  // has been changed, notifies observers, and clears the observer list.
  void OnModelFileChangedInternal();

  // For use with `BackgroundFile::ReplaceFile`.
  void ModelFileReplacedCallback();

  // The file that contains the language detection model. Available when the
  // file path has been provided by the Optimization Guide and has been
  // successfully loaded.
  BackgroundFile language_detection_model_file_;

  // Records whether we have ever explicitly set the model file (including to
  // an invalid value). Until this becomes true, requests for the file will be
  // queued.
  bool has_model_ever_been_set_ = false;

  // The set of callbacks associated with requests for the language detection
  // model. The callback notifies requesters than the model file is now
  // available and can be safely requested.
  std::vector<GetModelCallback> pending_model_requests_;

  base::WeakPtrFactory<LanguageDetectionModelProvider> weak_ptr_factory_{this};
};

}  //  namespace language_detection

#endif  // COMPONENTS_LANGUAGE_DETECTION_CORE_BROWSER_LANGUAGE_DETECTION_MODEL_PROVIDER_H_
