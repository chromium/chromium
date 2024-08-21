// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/archive_patch_helper.h"

#include <stdint.h>

#include <optional>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "chrome/installer/util/lzma_util.h"
#include "components/zucchini/zucchini.h"
#include "components/zucchini/zucchini_integration.h"
#include "third_party/bspatch/mbspatch.h"

namespace installer {

ArchivePatchHelper::ArchivePatchHelper(const base::FilePath& working_directory,
                                       const base::FilePath& compressed_archive,
                                       const base::FilePath& patch_source,
                                       const base::FilePath& target,
                                       UnPackConsumer consumer)
    : working_directory_(working_directory),
      compressed_archive_(compressed_archive),
      patch_source_(patch_source),
      target_(target),
      consumer_(consumer) {}

ArchivePatchHelper::~ArchivePatchHelper() {}

// static
bool ArchivePatchHelper::UncompressAndPatch(
    const base::FilePath& working_directory,
    const base::FilePath& compressed_archive,
    const base::FilePath& patch_source,
    const base::FilePath& target,
    UnPackConsumer consumer) {
  ArchivePatchHelper instance(working_directory, compressed_archive,
                              patch_source, target, consumer);
  return (instance.Uncompress(nullptr) && instance.ApplyAndDeletePatch());
}

bool ArchivePatchHelper::Uncompress(base::FilePath* last_uncompressed_file) {
  // The target shouldn't already exist.
  DCHECK(!base::PathExists(target_));

  // UnPackArchive takes care of logging.
  base::FilePath output_file;
  UnPackStatus unpack_status =
      UnPackArchive(compressed_archive_, working_directory_, &output_file);
  RecordUnPackMetrics(unpack_status, consumer_);
  if (unpack_status != UNPACK_NO_ERROR)
    return false;

  last_uncompressed_file_ = output_file;
  if (last_uncompressed_file)
    *last_uncompressed_file = last_uncompressed_file_;
  return true;
}

bool ArchivePatchHelper::ApplyAndDeletePatch() {
  const bool succeeded = ZucchiniEnsemblePatch() || BinaryPatch();
  if (!last_uncompressed_file_.empty()) {
    base::DeleteFile(last_uncompressed_file_);
  }
  return succeeded;
}

bool ArchivePatchHelper::ZucchiniEnsemblePatch() {
  if (last_uncompressed_file_.empty()) {
    LOG(ERROR) << "No patch file found in compressed archive.";
    return false;
  }

  zucchini::status::Code result =
      zucchini::Apply(patch_source_, last_uncompressed_file_, target_);

  if (result == zucchini::status::kStatusSuccess)
    return true;

  LOG(ERROR) << "Failed to apply patch " << last_uncompressed_file_.value()
             << " to file " << patch_source_.value() << " and generating file "
             << target_.value()
             << " using Zucchini. err=" << static_cast<uint32_t>(result);

  // Ensure a partial output is not left behind.
  base::DeleteFile(target_);

  return false;
}

bool ArchivePatchHelper::BinaryPatch() {
  if (last_uncompressed_file_.empty()) {
    LOG(ERROR) << "No patch file found in compressed archive.";
    return false;
  }

  int result = ApplyBinaryPatch(patch_source_.value().c_str(),
                                last_uncompressed_file_.value().c_str(),
                                target_.value().c_str());
  if (result == OK)
    return true;

  LOG(ERROR) << "Failed to apply patch " << last_uncompressed_file_.value()
             << " to file " << patch_source_.value() << " and generating file "
             << target_.value() << " using bsdiff. err=" << result;

  // Ensure a partial output is not left behind.
  base::DeleteFile(target_);

  return false;
}

}  // namespace installer
