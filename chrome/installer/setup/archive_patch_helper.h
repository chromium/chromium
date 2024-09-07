// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_SETUP_ARCHIVE_PATCH_HELPER_H_
#define CHROME_INSTALLER_SETUP_ARCHIVE_PATCH_HELPER_H_

#include "base/files/file_path.h"
#include "chrome/installer/setup/setup_util.h"
#include "chrome/installer/util/lzma_util.h"

namespace installer {

// A helper class that facilitates uncompressing and patching the chrome archive
// and installer.
//
// Chrome's installer is deployed along with a compressed archive containing
// either 1) an uncompressd archive of the product binaries or 2) a patch file
// to be applied to the uncompressed archive of the version being updated. To
// obtain the uncompressed archive, the contents of the compressed archive are
// uncompressed and extracted. Installation proceeds directly if the
// uncompressed archive is found after this step. Otherwise, the patch is
// applied to the previous version's uncompressed archive using either
// Zucchini's patching or bspatch.
//
// Chrome's installer itself may also be deployed as a patch against the
// previous version's saved installer binary. The same process is followed to
// obtain the new installer. The compressed archive unconditionally contains a
// patch file in this case.
class ArchivePatchHelper {
 public:
  // Constructs an instance that can uncompress |compressed_archive| into
  // |working_directory| and optionally apply the extracted patch file to
  // |patch_source|, writing the result to |target|.
  ArchivePatchHelper(const base::FilePath& working_directory,
                     const base::FilePath& compressed_archive,
                     const base::FilePath& patch_source,
                     const base::FilePath& target,
                     UnPackConsumer consumer);

  ArchivePatchHelper(const ArchivePatchHelper&) = delete;
  ArchivePatchHelper& operator=(const ArchivePatchHelper&) = delete;

  ~ArchivePatchHelper();

  // Uncompresses |compressed_archive| in |working_directory| then applies the
  // extracted patch file to |patch_source|, writing the result to |target|.
  // Ensemble patching via Zucchini is attempted first (if it is enabled). If
  // that fails bspatch is attempted. Returns false if uncompression or all
  // patching steps fail.
  static bool UncompressAndPatch(const base::FilePath& working_directory,
                                 const base::FilePath& compressed_archive,
                                 const base::FilePath& patch_source,
                                 const base::FilePath& target,
                                 UnPackConsumer consumer);

  // Uncompresses compressed_archive() into the working directory. On success,
  // last_uncompressed_file (if not nullptr) is populated with the path to the
  // last file extracted from the archive.
  bool Uncompress(base::FilePath* last_uncompressed_file);

  // Performs ensemble patching on the uncompressed version of
  // |compressed_archive| in |working_directory| as specified in the constructor
  // using files from |patch_source|. Ensemble patching via Zucchini is
  // attempted first (if it is enabled). Zucchini falls back to bspatch if
  // unsuccessful. The uncompressed patch file is unconditionally deleted at the
  // end.
  bool ApplyAndDeletePatch();

  // Attempts to use Zucchini to apply last_uncompressed_file() to
  // patch_source() to generate target(). Returns false if patching fails.
  bool ZucchiniEnsemblePatch();

  // Attempts to use bspatch to apply last_uncompressed_file() to patch_source()
  // to generate target(). Returns false if patching fails.
  bool BinaryPatch();

  const base::FilePath& compressed_archive() const {
    return compressed_archive_;
  }
  void set_patch_source(const base::FilePath& patch_source) {
    patch_source_ = patch_source;
  }
  const base::FilePath& patch_source() const { return patch_source_; }
  const base::FilePath& target() const { return target_; }

  // Returns the path of the last file extracted by Uncompress().
  const base::FilePath& last_uncompressed_file() const {
    return last_uncompressed_file_;
  }
  void set_last_uncompressed_file(
      const base::FilePath& last_uncompressed_file) {
    last_uncompressed_file_ = last_uncompressed_file;
  }

 private:
  base::FilePath working_directory_;
  base::FilePath compressed_archive_;
  base::FilePath patch_source_;
  base::FilePath target_;
  base::FilePath last_uncompressed_file_;
  UnPackConsumer consumer_;
};

}  // namespace installer

#endif  // CHROME_INSTALLER_SETUP_ARCHIVE_PATCH_HELPER_H_
