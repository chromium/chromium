// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FILE_ACCESS_FILE_ACCESS_COPY_OR_MOVE_DELEGATE_FACTORY_H_
#define COMPONENTS_FILE_ACCESS_FILE_ACCESS_COPY_OR_MOVE_DELEGATE_FACTORY_H_

#include <memory>

#include "base/component_export.h"

namespace storage {

class CopyOrMoveHookDelegate;
}

namespace file_access {

// This class is an interface for a factory, which creates specific instances of
// the CopyOrMoveHookDelegate. Methods of this interface are expected to be
// called from the IO thread. Only one instance of a class which extends
// this class can exist at a time. The class itself also manages this one
// instance. When it is replaced the old instance is destructed. The class is
// used in the copy operation to be able to inject in the different copy stages
// (begin, process, end).
class COMPONENT_EXPORT(FILE_ACCESS) FileAccessCopyOrMoveDelegateFactory {
 public:
  FileAccessCopyOrMoveDelegateFactory(
      const FileAccessCopyOrMoveDelegateFactory&) = delete;
  FileAccessCopyOrMoveDelegateFactory& operator=(
      const FileAccessCopyOrMoveDelegateFactory&) = delete;

  // Returns a pointer to the existing instance of the class.
  static FileAccessCopyOrMoveDelegateFactory* Get();

  // Returns true if an instance exists, without forcing an initialization.
  static bool HasInstance();

  // Deletes the existing instance of the class if it's already created.
  // Indicates that restricting data transfer is no longer required.
  // The instance will be deconstructed
  static void DeleteInstance();

  virtual std::unique_ptr<storage::CopyOrMoveHookDelegate> MakeHook() = 0;

 protected:
  friend class FileAccessCopyOrMoveDelegateFactoryTest;

  FileAccessCopyOrMoveDelegateFactory();

  virtual ~FileAccessCopyOrMoveDelegateFactory();

  static FileAccessCopyOrMoveDelegateFactory*
      file_access_copy_or_move_delegate_factory_;
};

}  // namespace file_access

#endif  // COMPONENTS_FILE_ACCESS_FILE_ACCESS_COPY_OR_MOVE_DELEGATE_FACTORY_H_
