// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Component updates can be either differential updates or full updates.
// Full updates come in CRX format; differential updates come in CRX-style
// archives, but have a different magic number. They contain "commands.json", a
// list of commands for the patcher to follow. The patcher uses these commands,
// the other files in the archive, and the files from the existing installation
// of the component to create the contents of a full update, which is then
// installed normally.
// Component updates are specified by the 'codebasediff' attribute of an
// updatecheck response:
//   <updatecheck codebase="http://example.com/extension_1.2.3.4.crx"
//                hash="12345" size="9854" status="ok" version="1.2.3.4"
//                prodversionmin="2.0.143.0"
//                codebasediff="http://example.com/diff_1.2.3.4.crx"
//                hashdiff="123" sizediff="101"
//                fp="1.123"/>
// The component updater will attempt a differential update if it is available
// and allowed to, and fall back to a full update if it fails.
//
// After installation (diff or full), the component updater records "fp", the
// fingerprint of the installed files, to later identify the existing files to
// the server so that a proper differential update can be provided next cycle.

#ifndef COMPONENTS_UPDATE_CLIENT_COMPONENT_PATCHER_H_
#define COMPONENTS_UPDATE_CLIENT_COMPONENT_PATCHER_H_

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "components/update_client/component_unpacker.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// TODO(crbug.com/1349158): Remove this class once Puffin patches are fully
// implemented.

namespace base {
class FilePath;
}

namespace update_client {

class CrxInstaller;
class DeltaUpdateOp;
class Patcher;
enum class UnpackerError;

// The type of a patch file.
enum PatchType {
  kPatchTypeUnknown,
  kPatchTypeCourgette,
  kPatchTypeBsdiff,
};

// Encapsulates a task for applying a differential update to a component.
class ComponentPatcher : public base::RefCountedThreadSafe<ComponentPatcher> {
 public:
  using Callback = base::OnceCallback<void(UnpackerError, int)>;

  // Takes an unpacked differential CRX (|input_dir|) and a component installer,
  // and sets up the class to create a new (non-differential) unpacked CRX.
  // If |in_process| is true, patching will be done completely within the
  // existing process. Otherwise, some steps of patching may be done
  // out-of-process.
  ComponentPatcher(const base::FilePath& input_dir,
                   const base::FilePath& unpack_dir,
                   scoped_refptr<CrxInstaller> installer,
                   scoped_refptr<Patcher> patcher);

  ComponentPatcher(const ComponentPatcher&) = delete;
  ComponentPatcher& operator=(const ComponentPatcher&) = delete;

  // Starts patching files. This member function returns immediately, after
  // posting a task to do the patching. When patching has been completed,
  // |callback| will be called with the error codes if any error codes were
  // encountered.
  void Start(Callback callback);

 private:
  friend class base::RefCountedThreadSafe<ComponentPatcher>;

  virtual ~ComponentPatcher();

  void StartPatching();

  void PatchNextFile();

  void DonePatchingFile(UnpackerError error, int extended_error);

  void DonePatching(UnpackerError error, int extended_error);

  const base::FilePath input_dir_;
  const base::FilePath unpack_dir_;
  scoped_refptr<CrxInstaller> installer_;
  scoped_refptr<Patcher> patcher_;
  Callback callback_;
  absl::optional<base::Value::List> commands_;
  base::Value::List::const_iterator next_command_;
  scoped_refptr<DeltaUpdateOp> current_operation_;
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_COMPONENT_PATCHER_H_
