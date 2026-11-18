// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_CANCELLATION_H_
#define COMPONENTS_UPDATE_CLIENT_CANCELLATION_H_

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"

namespace update_client {

// How cancellation treats partial download state.
enum class CancelMode {
  // Deletes partial download state. Use when the update won't be retried
  // soon, for example on stop or uninstall.
  kDeletePartialDownload,
  // Asks the downloader to keep partial download state so a later update can
  // reuse it, for example on a metered network. Downloaders that can't keep
  // state treat this as `kDeletePartialDownload`.
  kKeepPartialDownload,
};

using CancelCallback = base::OnceCallback<void(CancelMode)>;

// Some update_client operations may be cancelled by the user while underway.
// When this happens, update_client conducts an orderly tear-down by
// interrupting ongoing work, and returning via the normal control flow (but
// most likely with error results instead of useful results).
//
// Depending on how much progress the operation has made, different steps may
// be required to interrupt and cancel ongoing work. A `Cancellation` is a
// container for these steps. When a function starts interruptible work, it
// should add a callback to interrupt the work to a `Cancellation` using
// `OnCancel`. When the work is completed or can no longer be interrupted, it
// should `Clear` the `Cancellation`. All functions of `Cancellation` must be
// called on the sequence on which the `Cancellation` was created.
class Cancellation : public base::RefCountedThreadSafe<Cancellation> {
 public:
  Cancellation();
  Cancellation(const Cancellation&) = delete;
  Cancellation& operator=(const Cancellation&) = delete;

  // Same as `CancelWithMode(CancelMode::kDeletePartialDownload)`.
  void Cancel();

  // Triggers cancellation with `mode`. Only the first call's `mode` is used;
  // subsequent calls are no-ops.
  void CancelWithMode(CancelMode mode);

  // Returns whether cancellation was triggered.
  [[nodiscard]] bool IsCancelled();

  // Registers `callback` to run on cancel (or runs it immediately if already
  // cancelled), replacing any previously registered callback. `callback`
  // receives the mode of the first cancellation.
  void OnCancel(CancelCallback callback);

  // Unregisters the cancellation callback.
  void Clear();

 private:
  friend class base::RefCountedThreadSafe<Cancellation>;
  ~Cancellation();

  SEQUENCE_CHECKER(sequence_checker_);
  bool cancelled_ = false;
  CancelMode cancel_mode_ = CancelMode::kDeletePartialDownload;
  CancelCallback task_;
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_CANCELLATION_H_
