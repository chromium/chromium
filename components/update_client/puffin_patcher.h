// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Component updates can be either differential updates or full updates.
// Full updates come in CRX format; differential puffdiff updates come in *.puff
// files, with a different magic number. They describe a bsdiff diff generated
// by Puffin. The patcher takes the previous version of this binary, applies
// puffpatch and generates the newest version of the CRX.
//
// The component updater attempts a differential update if it is available
// and allowed to, and fall back to a full update if it fails.
//
// After installation (diff or full), the component updater records "fp", the
// fingerprint of the installed files, to later identify the existing files to
// the server so that a proper differential update can be provided next cycle.

#ifndef COMPONENTS_UPDATE_CLIENT_PUFFIN_PATCHER_H_
#define COMPONENTS_UPDATE_CLIENT_PUFFIN_PATCHER_H_

#include "base/files/file.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "components/update_client/component_unpacker.h"

namespace base {
class File;
}

namespace update_client {

class Patcher;

// Encapsulates a task for applying a differential update to a component.
class PuffinPatcher : public base::RefCountedThreadSafe<PuffinPatcher> {
 public:
  PuffinPatcher(const PuffinPatcher&) = delete;
  PuffinPatcher& operator=(const PuffinPatcher&) = delete;

  // Takes a full CRX `old_crx_file` and a Puffin puffpatch file
  // `puff_patch_file`, and sets up the class to create a new full
  // If `in_process` is true, patching is done completely within the
  // existing process. Otherwise, some steps of patching may be done
  // out-of-process. Then, it starts patching files.
  //
  // This static function returns immediately, after posting a task
  // to do the patching. When patching has been completed,
  // `callback` is called with the error codes if any error codes were
  // encountered.
  static void Patch(base::File old_crx_file,
                    base::File puff_patch_file,
                    base::File new_crx_output,
                    scoped_refptr<Patcher> patcher,
                    base::OnceCallback<void(UnpackerError, int)> callback);

 private:
  friend class base::RefCountedThreadSafe<PuffinPatcher>;

  PuffinPatcher(base::File old_crx_file,
                base::File puff_patch_file,
                base::File new_crx_output,
                scoped_refptr<Patcher> patcher,
                base::OnceCallback<void(UnpackerError, int)> callback);

  virtual ~PuffinPatcher();

  void StartPatching();

  void PatchCrx();

  void DonePatch(int extended_error);

  void DonePatching(UnpackerError error, int extended_error);

  base::File old_crx_file_;
  base::File puff_patch_file_;
  base::File new_crx_output_file_;
  scoped_refptr<Patcher> patcher_;
  base::OnceCallback<void(UnpackerError, int)> callback_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_PUFFIN_PATCHER_H_
