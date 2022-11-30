// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SAFE_BROWSING_MACH_O_IMAGE_READER_MAC_H_
#define CHROME_COMMON_SAFE_BROWSING_MACH_O_IMAGE_READER_MAC_H_

#include <mach-o/loader.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

namespace safe_browsing {

class ByteSlice;

// MachOImageReader is used to extract information about a Mach-O binary image.
// This class supports fat and thin images. Initialize() must be called before
// any other methods; if it returns false, it is illegal to call any other
// methods on this class.
class MachOImageReader {
 public:
  // Represents a Mach-O load command, including all of its data.
  struct LoadCommand {
    LoadCommand();
    LoadCommand(const LoadCommand& other);
    ~LoadCommand();

    uint32_t cmd() const {
      return as_command<load_command>()->cmd;
    }

    uint32_t cmdsize() const {
      return as_command<load_command>()->cmdsize;
    }

    template <typename T>
    const T* as_command() const {
      if (data.size() < sizeof(T))
        return nullptr;
      return reinterpret_cast<const T*>(&data[0]);
    }

    std::vector<uint8_t> data;
  };

  // Returns true if |magic| is any Mach-O magic number. This can be used on the
  // first four bytes of a file (either in little- or big-endian) to quickly
  // determine whether or not the file is potentially a Mach-O file. An instance
  // of this class must be used for a true validity check.
  static bool IsMachOMagicValue(uint32_t magic);

  MachOImageReader();

  MachOImageReader(const MachOImageReader&) = delete;
  MachOImageReader& operator=(const MachOImageReader&) = delete;

  ~MachOImageReader();

  // Initializes the instance and verifies that the data is a valid Mach-O
  // image. This does not take ownership of the bytes, so the data must
  // remain valid for the lifetime of this object. Returns true if the
  // instance is initialized and valid, false if the file could not be parsed
  // as a Mach-O image.
  bool Initialize(const uint8_t* image, size_t image_size);

  // Returns whether this is a fat Mach-O image. If this returns true, it is
  // only valid to call GetFatImages() and none of the other methods.
  bool IsFat();

  // It is only valid to call this method if IsFat() returns true. This
  // returns an image reader for each architecture in the fat file.
  std::vector<MachOImageReader*> GetFatImages();

  // Returns whether the image is a 64-bit image.
  bool Is64Bit();

  // Retrieves the mach_header structure for the appropriate architecture.
  const mach_header* GetMachHeader();
  const mach_header_64* GetMachHeader64();

  // Returns the Mach-O filetype field from the header.
  uint32_t GetFileType();

  // Returns an array of all the load commands in the image.
  const std::vector<MachOImageReader::LoadCommand>& GetLoadCommands();

  // If the image has a LC_CODE_SIGNATURE command, this retreives the code
  // signature blob in the __LINKEDIT segment.
  bool GetCodeSignatureInfo(std::vector<uint8_t>* info);

 private:
  std::unique_ptr<ByteSlice> data_;

  bool is_fat_;
  std::vector<std::unique_ptr<MachOImageReader>> fat_images_;

  bool is_64_bit_;
  std::vector<LoadCommand> commands_;
};

}  // namespace safe_browsing

#endif  // CHROME_COMMON_SAFE_BROWSING_MACH_O_IMAGE_READER_MAC_H_
