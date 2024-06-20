// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_MAC_ICNS_ENCODER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_MAC_ICNS_ENCODER_H_

#include <vector>

#include "base/containers/span.h"

namespace base {
class File;
class FilePath;
}  // namespace base

namespace gfx {
class Image;
}

namespace web_app {

// This class can be used to construct an icon in .icns file format. Create
// an instance, repeatedly call `AddImage` to add various sizes of the icon, and
// finally call `WriteToFile` to write the .icns file to disk.
class IcnsEncoder {
 public:
  IcnsEncoder();
  ~IcnsEncoder();

  // Adds another representation of the same icon to this encoder. Returns false
  // if the image can not be added to a .icns file, for example because it has
  // the wrong dimensions, the wrong color type, or because encoding failed.
  bool AddImage(const gfx::Image& image);

  // Writes all the images that were successfully added to this encoder to
  // `path` in the .icns file format. Returns false if saving to disk failed for
  // some reason.
  bool WriteToFile(const base::FilePath& path) const;

  // Appends a run-length-encoded copy of `data` to `rle_data`, using the
  // encoding scheme used by the .icns file format. Public for testing.
  static void AppendRLEImageData(base::span<const uint8_t> data,
                                 std::vector<uint8_t>* rle_data);

 private:
  // The .icns file format is made up of blocks, consistent of a type and data.
  // This struct represents such a block.
  struct Block {
    explicit Block(uint32_t type, std::vector<uint8_t> data = {});
    ~Block();
    Block(Block&&);
    Block& operator=(Block&&);

    // Type of this block.
    uint32_t type;
    // Data contained in the block, not including the header.
    std::vector<uint8_t> data;
  };

  // Number of bytes a single block header takes up.
  static constexpr size_t kBlockHeaderSize = 8;

  // Adds a block to the list of blocks making up this .icns file.
  void AppendBlock(uint32_t type, std::vector<uint8_t> data);

  // Writes `block` to `file`, including its header.
  static bool WriteBlockToFile(base::File& file, const Block& block);

  // The blocks making up this .icns file, not including the table of contents
  // block that will also be written to a file.
  std::vector<Block> blocks_;
  // The total size of all the blocks in `blocks_`. This also includes the size
  // of the headers for these blocks.
  size_t total_block_size_ = 0;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_OS_INTEGRATION_MAC_ICNS_ENCODER_H_
