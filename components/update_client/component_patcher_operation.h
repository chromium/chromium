// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_COMPONENT_PATCHER_OPERATION_H_
#define COMPONENTS_UPDATE_CLIENT_COMPONENT_PATCHER_OPERATION_H_

#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/update_client/component_patcher.h"
#include "components/update_client/component_unpacker.h"

namespace base {
class DictionaryValue;
}  // namespace base

namespace update_client {

extern const char kOp[];
extern const char kBsdiff[];
extern const char kCourgette[];
extern const char kInput[];
extern const char kPatch[];

class CrxInstaller;
class Patcher;
enum class UnpackerError;

class DeltaUpdateOp : public base::RefCountedThreadSafe<DeltaUpdateOp> {
 public:
  DeltaUpdateOp();

  // Parses, runs, and verifies the operation. Calls |callback| with the
  // result of the operation. The callback is called using |task_runner|.
  void Run(const base::DictionaryValue* command_args,
           const base::FilePath& input_dir,
           const base::FilePath& unpack_dir,
           scoped_refptr<CrxInstaller> installer,
           ComponentPatcher::Callback callback);

 protected:
  virtual ~DeltaUpdateOp();

  std::string output_sha256_;
  base::FilePath output_abs_path_;

 private:
  friend class base::RefCountedThreadSafe<DeltaUpdateOp>;

  UnpackerError CheckHash();

  // Subclasses must override DoParseArguments to parse operation-specific
  // arguments. DoParseArguments returns DELTA_OK on success; any other code
  // represents failure.
  virtual UnpackerError DoParseArguments(
      const base::DictionaryValue* command_args,
      const base::FilePath& input_dir,
      scoped_refptr<CrxInstaller> installer) = 0;

  // Subclasses must override DoRun to actually perform the patching operation.
  // They must call the provided callback when they have completed their
  // operations. In practice, the provided callback is always for "DoneRunning".
  virtual void DoRun(ComponentPatcher::Callback callback) = 0;

  // Callback given to subclasses for when they complete their operation.
  // Validates the output, and posts a task to the patching operation's
  // callback.
  void DoneRunning(UnpackerError error, int extended_error);

  ComponentPatcher::Callback callback_;

  DISALLOW_COPY_AND_ASSIGN(DeltaUpdateOp);
};

// A 'copy' operation takes a file currently residing on the disk and moves it
// into the unpacking directory: this represents "no change" in the file being
// installed.
class DeltaUpdateOpCopy : public DeltaUpdateOp {
 public:
  DeltaUpdateOpCopy();

 private:
  ~DeltaUpdateOpCopy() override;

  // Overrides of DeltaUpdateOp.
  UnpackerError DoParseArguments(
      const base::DictionaryValue* command_args,
      const base::FilePath& input_dir,
      scoped_refptr<CrxInstaller> installer) override;

  void DoRun(ComponentPatcher::Callback callback) override;

  base::FilePath input_abs_path_;

  DISALLOW_COPY_AND_ASSIGN(DeltaUpdateOpCopy);
};

// A 'create' operation takes a full file that was sent in the delta update
// archive and moves it into the unpacking directory: this represents the
// addition of a new file, or a file so different that no bandwidth could be
// saved by transmitting a differential update.
class DeltaUpdateOpCreate : public DeltaUpdateOp {
 public:
  DeltaUpdateOpCreate();

 private:
  ~DeltaUpdateOpCreate() override;

  // Overrides of DeltaUpdateOp.
  UnpackerError DoParseArguments(
      const base::DictionaryValue* command_args,
      const base::FilePath& input_dir,
      scoped_refptr<CrxInstaller> installer) override;

  void DoRun(ComponentPatcher::Callback callback) override;

  base::FilePath patch_abs_path_;

  DISALLOW_COPY_AND_ASSIGN(DeltaUpdateOpCreate);
};

// Both 'bsdiff' and 'courgette' operations take an existing file on disk,
// and a bsdiff- or Courgette-format patch file provided in the delta update
// package, and run bsdiff or Courgette to construct an output file in the
// unpacking directory.
class DeltaUpdateOpPatch : public DeltaUpdateOp {
 public:
  DeltaUpdateOpPatch(const std::string& operation,
                     scoped_refptr<Patcher> patcher);

 private:
  ~DeltaUpdateOpPatch() override;

  // Overrides of DeltaUpdateOp.
  UnpackerError DoParseArguments(
      const base::DictionaryValue* command_args,
      const base::FilePath& input_dir,
      scoped_refptr<CrxInstaller> installer) override;

  void DoRun(ComponentPatcher::Callback callback) override;

  // |success_code| is the code that indicates a successful patch.
  // |result| is the code the patching operation returned.
  void DonePatching(ComponentPatcher::Callback callback, int result);

  std::string operation_;
  scoped_refptr<Patcher> patcher_;
  base::FilePath patch_abs_path_;
  base::FilePath input_abs_path_;

  DISALLOW_COPY_AND_ASSIGN(DeltaUpdateOpPatch);
};

DeltaUpdateOp* CreateDeltaUpdateOp(const std::string& operation,
                                   scoped_refptr<Patcher> patcher);

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_COMPONENT_PATCHER_OPERATION_H_
