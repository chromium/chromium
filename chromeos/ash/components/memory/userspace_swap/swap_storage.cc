// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chromeos/ash/components/memory/userspace_swap/swap_storage.h"

#include <fcntl.h>
#include <linux/userfaultfd.h>
#include <poll.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/vfs.h>
#include <unistd.h>
#include <cstring>

#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/numerics/checked_math.h"
#include "base/path_service.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/process_metrics.h"
#include "base/threading/scoped_blocking_call.h"
#include "crypto/aead.h"
#include "crypto/random.h"
#include "third_party/zlib/google/compression_utils.h"

namespace ash {
namespace memory {
namespace userspace_swap {

namespace {

// Adds a compression layer to a SwapFile.
class CompressedSwapFile : public SwapFile {
 public:
  CompressedSwapFile(const CompressedSwapFile&) = delete;
  CompressedSwapFile& operator=(const CompressedSwapFile&) = delete;

  ~CompressedSwapFile() override;

  // SwapFile impl:
  bool WriteToSwap(const Region& src, Region* swap_region) override;
  ssize_t ReadFromSwap(const Region& swap_region, const Region& dest) override;

 protected:
  friend class SwapFile;
  friend class EncryptedCompressedSwapFile;

  explicit CompressedSwapFile(base::ScopedFD fd);

  // Compress will compress the region |src| into the region |dest| returning
  // true if successful. Upon successful completion |compressed_size| will
  // contain the number of compressed bytes written to |dest|.
  static bool Compress(const Region& src,
                       const Region& dest,
                       size_t* compressed_size);

  // Decompress will decompress the region |src| into |dest|. It is expected
  // that |dest| be large enough to hold the decompressed buffer, the return
  // value is the number of decompressed bytes.
  static ssize_t Decompress(const Region& src, const Region& dest);
};

// Adds an encryption layer to a SwapFile.
class EncryptedSwapFile : public SwapFile {
 public:
  EncryptedSwapFile(const EncryptedSwapFile&) = delete;
  EncryptedSwapFile& operator=(const EncryptedSwapFile&) = delete;

  ~EncryptedSwapFile() override;

  // SwapFile impl:
  bool WriteToSwap(const Region& src, Region* swap_region) override;
  ssize_t ReadFromSwap(const Region& swap_region, const Region& dest) override;

 protected:
  friend class SwapFile;

  explicit EncryptedSwapFile(base::ScopedFD fd);

  // This key and nonce are random and ephemeral.
  crypto::Aead aead_;
  std::vector<uint8_t> key_;
  std::vector<uint8_t> nonce_;
};

// Adds a encryption layer to a compressed swap file.
class EncryptedCompressedSwapFile : public EncryptedSwapFile {
 public:
  EncryptedCompressedSwapFile(const EncryptedCompressedSwapFile&) = delete;
  EncryptedCompressedSwapFile& operator=(const EncryptedCompressedSwapFile&) =
      delete;

  ~EncryptedCompressedSwapFile() override;

  // SwapFile impl:
  bool WriteToSwap(const Region& src, Region* swap_region) override;
  ssize_t ReadFromSwap(const Region& swap_region, const Region& dest) override;

 protected:
  friend class SwapFile;

  explicit EncryptedCompressedSwapFile(base::ScopedFD fd);
};

// Because for some inputs that aren't compressible it can result in a size
// that's slightly larger, we allow for this.
constexpr size_t kCompressionExtra = 32 << 10;

}  // namespace

// Static
std::unique_ptr<SwapFile> SwapFile::Create(Type type) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  // We enforce that the file is encrypted.
  CHECK(type & Type::kEncrypted);

  base::FilePath directory;
  if (!GetDirectoryForSwapFile(&directory)) {
    return nullptr;
  }

  // We open the file with O_TMPFILE which creates an unnamed inode and anything
  // written to the file will be lost when this fd is closed. O_EXCL prevents
  // this file from being linked to the filesystem. Note, O_EXCL behaves
  // differently because of O_TMPFILE. For more information on this see man 2
  // open.
  base::ScopedFD swap_fd(HANDLE_EINTR(
      open(directory.value().c_str(), O_TMPFILE | O_EXCL | O_RDWR | O_CLOEXEC,
           S_IRUSR | S_IWUSR)));

  if (!swap_fd.is_valid()) {
    PLOG(ERROR) << "Unable to open a temporary swap file in " << directory;
    return nullptr;
  }

  return SwapFile::WrapFD(std::move(swap_fd), type);
}

std::unique_ptr<SwapFile> SwapFile::WrapFD(base::ScopedFD swap_fd, Type type) {
  std::unique_ptr<SwapFile> swap;
  if (type == (Type::kCompressed | Type::kEncrypted)) {
    swap.reset(new EncryptedCompressedSwapFile(std::move(swap_fd)));
  } else if (type == Type::kCompressed) {
    swap.reset(new CompressedSwapFile(std::move(swap_fd)));
  } else if (type == Type::kEncrypted) {
    swap.reset(new EncryptedSwapFile(std::move(swap_fd)));
  } else {
    swap.reset(new SwapFile(std::move(swap_fd)));
  }

  return swap;
}

SwapFile::~SwapFile() = default;
SwapFile::SwapFile(base::ScopedFD fd) : fd_(std::move(fd)) {}

base::ScopedFD SwapFile::ReleaseFD() {
  return std::move(fd_);
}

uint64_t SwapFile::GetUsageKB() const {
  struct stat statbuf = {};
  fstat(fd_.get(), &statbuf);
  // fstat returns the number of 512byte blocks, we convert to KB.
  return (statbuf.st_blocks * 512) >> 10;
}

// Static
bool SwapFile::GetDirectoryForSwapFile(base::FilePath* file_path) {
  // We cache the file path so we don't have to repeatedly call these functions.
  static base::FilePath cached_path = []() -> base::FilePath {
    // We try to look for the unecrypted swap folder first, if it doesn't exist
    // we will fall back the user's home directory. If that happens it means
    // we're encrypted before writing to an encrypted file system so we log a
    // warning.
    const base::FilePath swap_folder(
        "/mnt/stateful_partition/unencrypted/userspace_swap.tmp/");
    if (base::PathExists(swap_folder)) {
      return swap_folder;
    }

    PLOG(WARNING) << "Swap folder " << swap_folder
                  << " did not exist so userspace swap will be be disabled";
    return base::FilePath();
  }();

  if (!cached_path.empty()) {
    *file_path = cached_path;
    return true;
  }

  return false;
}

// Static
uint64_t SwapFile::GetBackingStoreFreeSpaceKB() {
  base::FilePath swap_file_dir;
  if (!GetDirectoryForSwapFile(&swap_file_dir)) {
    return 0;
  }

  struct statfs buf = {};
  if (statfs(swap_file_dir.value().c_str(), &buf) < 0) {
    PLOG(ERROR) << "Unable to get backing store space freespace for swap";
    return 0;
  }

  // Convert number of blocks to KB.
  return (buf.f_bavail * buf.f_bsize) >> 10;
}

bool SwapFile::WriteToSwap(const Region& src, Region* swap_region) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  // Writes are the only operations that cannot happen concurrently. Writes use
  // write(2) and adjust the file pointer so this is a critical section. Reads
  // and drops use position pread(2)/fallocate(2) and can safely be performed
  // concurrently as they only access those regions and do not affect the file
  // pointer.
  base::AutoLock scoped_lock(write_lock_);

  // We capture the current file pointer to determine where we started writing
  // at.
  DCHECK(swap_region);
  swap_region->address = lseek(fd_.get(), 0, SEEK_CUR);
  swap_region->length = 0;

  while (swap_region->length < src.length) {
    int bytes_written = HANDLE_EINTR(write(
        fd_.get(), reinterpret_cast<char*>(src.address) + swap_region->length,
        src.length - swap_region->length));

    if (bytes_written <= 0) {
      // We want the user to see errno from the write(2) call and not from
      // lseek(2) should it also fail.
      int write_failed_errno = errno;

      // Seek back the file pointer as anything partially written is not
      // tracked and would have been wasted file space.
      lseek(fd_.get(), swap_region->address, SEEK_SET);

      // We want the user to see errno from the write(2) call and not from
      // lseek(2).
      errno = write_failed_errno;
      swap_region->address = 0;
      swap_region->length = 0;
      return false;
    }

    swap_region->length += bytes_written;
  }

  return true;
}

ssize_t SwapFile::ReadFromSwap(const Region& swap_region, const Region& dest) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  CHECK_EQ(swap_region.length, dest.length);

  uint64_t bytes_read = 0;
  while (bytes_read < swap_region.length) {
    int64_t res = HANDLE_EINTR(pread(
        fd_.get(), reinterpret_cast<char*>(dest.address) + bytes_read,
        swap_region.length - bytes_read, swap_region.address + bytes_read));
    if (res <= 0) {
      return res;
    }

    bytes_read += res;
  }
  return bytes_read;
}

bool SwapFile::DropFromSwap(const Region& swap_region) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  int res = HANDLE_EINTR(fallocate(fd_.get(),
                                   FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE,
                                   swap_region.address, swap_region.length));
  if (res < 0) {
    return false;
  }

  return true;
}

CompressedSwapFile::~CompressedSwapFile() = default;

CompressedSwapFile::CompressedSwapFile(base::ScopedFD fd)
    : SwapFile(std::move(fd)) {}

// Static
inline bool CompressedSwapFile::Compress(const Region& src,
                                         const Region& dest,
                                         size_t* compressed_size) {
  return compression::GzipCompress(src.AsStringPiece(),
                                   reinterpret_cast<char*>(dest.address),
                                   dest.length, compressed_size,
                                   /* malloc_fn= */ nullptr,
                                   /* free_fn= */ nullptr);
}

bool CompressedSwapFile::WriteToSwap(const Region& src, Region* swap_region) {
  // We use a larger buffer because in some situations the compression can
  // actually be larger than the input, while this is very rare we allow for it.
  uint64_t buf_size =
      (base::CheckedNumeric<uint64_t>(src.length) + kCompressionExtra)
          .ValueOrDie();
  std::vector<uint8_t> compressed_buf(buf_size);

  size_t compressed_size = 0;

  // Compress src into compressed buf.
  if (!CompressedSwapFile::Compress(src, Region(compressed_buf),
                                    &compressed_size)) {
    errno = EIO;
    return false;
  }
  compressed_buf.resize(compressed_size);

  // Now write our compressed buffer to disk.
  return SwapFile::WriteToSwap(Region(compressed_buf), swap_region);
}

// Static
inline ssize_t CompressedSwapFile::Decompress(const Region& src,
                                              const Region& dest) {
  uint32_t uncompressed_size =
      compression::GetUncompressedSize(src.AsStringPiece());
  CHECK_EQ(dest.length, uncompressed_size);

  if (!compression::GzipUncompress(src.AsStringPiece(), dest.AsStringPiece())) {
    errno = EIO;
    return -1;
  }

  return uncompressed_size;
}

ssize_t CompressedSwapFile::ReadFromSwap(const Region& swap_region,
                                         const Region& dest) {
  // Read from disk and then decompress directly into the buffer.
  std::vector<uint8_t> compressed_buf(swap_region.length);

  ssize_t read_res =
      SwapFile::ReadFromSwap(swap_region, Region(compressed_buf));
  if (read_res != static_cast<ssize_t>(swap_region.length)) {
    return read_res;
  }
  compressed_buf.resize(read_res);

  return CompressedSwapFile::Decompress(Region(compressed_buf), dest);
}

EncryptedSwapFile::~EncryptedSwapFile() {
  memset(key_.data(), 0, key_.size());
  memset(nonce_.data(), 0, nonce_.size());
}

EncryptedSwapFile::EncryptedSwapFile(base::ScopedFD fd)
    : SwapFile(std::move(fd)), aead_(crypto::Aead::AES_256_GCM_SIV) {
  key_.resize(aead_.KeyLength());
  nonce_.resize(aead_.NonceLength());

  CHECK_EQ(aead_.KeyLength(), key_.size());
  CHECK_EQ(aead_.NonceLength(), nonce_.size());

  crypto::RandBytes(nonce_);
  crypto::RandBytes(key_);

  aead_.Init(key_);
}

bool EncryptedSwapFile::WriteToSwap(const Region& src, Region* swap_region) {
  std::vector<uint8_t> cipher_text =
      aead_.Seal(src.AsSpan<const uint8_t>(), nonce_,
                 /* additional data */ base::span<const uint8_t>());
  if (cipher_text.empty()) {
    LOG(ERROR) << "Unable to encrypt region";
    errno = EIO;
    return false;
  }

  // Write the encrypted contents to disk.
  return SwapFile::WriteToSwap(Region(cipher_text), swap_region);
}

ssize_t EncryptedSwapFile::ReadFromSwap(const Region& swap_region,
                                        const Region& dest) {
  // Start by reading the contents from the swap file and then decrypt it.
  std::vector<uint8_t> cipher_text(swap_region.length);
  ssize_t read_bytes = SwapFile::ReadFromSwap(swap_region, Region(cipher_text));
  if (read_bytes != static_cast<ssize_t>(swap_region.length)) {
    errno = EIO;
    return -1;
  }
  cipher_text.resize(read_bytes);

  std::optional<std::vector<uint8_t>> decrypted =
      aead_.Open(cipher_text, nonce_,
                 /* additional data */ base::span<const uint8_t>());
  if (!decrypted) {
    LOG(ERROR) << "Decryption failure";
    errno = EIO;
    return -1;
  }

  if (dest.length < decrypted.value().size()) {
    LOG(ERROR) << "Decryption buffer too small";
    errno = ENOMEM;
    return -1;
  }

  memcpy(reinterpret_cast<void*>(dest.address), decrypted.value().data(),
         decrypted.value().size());
  return decrypted.value().size();
}

EncryptedCompressedSwapFile::~EncryptedCompressedSwapFile() = default;
EncryptedCompressedSwapFile::EncryptedCompressedSwapFile(base::ScopedFD fd)
    : EncryptedSwapFile(std::move(fd)) {}

ssize_t EncryptedCompressedSwapFile::ReadFromSwap(const Region& swap_region,
                                                  const Region& dest) {
  // First read from the encrypted swap file then decompress. Because
  // compression may have resulted in a size which is larger than the original
  // payload for some rare inputs we allow for this.
  uint64_t buf_size =
      (base::CheckedNumeric<uint64_t>(dest.length) + kCompressionExtra)
          .ValueOrDie();
  std::vector<uint8_t> compressed_buf(buf_size);

  ssize_t read_res =
      EncryptedSwapFile::ReadFromSwap(swap_region, Region(compressed_buf));
  if (read_res == -1) {
    PLOG(ERROR) << "Read failed " << read_res;
    return read_res;
  }
  compressed_buf.resize(read_res);

  // Decompress directly into the destination region.
  return CompressedSwapFile::Decompress(Region(compressed_buf), dest);
}

bool EncryptedCompressedSwapFile::WriteToSwap(const Region& src,
                                              Region* swap_region) {
  // First compress the memory and then write to encrypted swap file.

  // We use a larger buffer because in some situations the compression can
  // actually be larger than the input, while this is very rare we allow for it.
  uint64_t buf_size =
      (base::CheckedNumeric<uint64_t>(src.length) + kCompressionExtra)
          .ValueOrDie();
  std::vector<uint8_t> compressed_buf(buf_size);
  size_t compressed_size = 0;

  if (!CompressedSwapFile::Compress(src, Region(compressed_buf),
                                    &compressed_size)) {
    return false;
  }
  compressed_buf.resize(compressed_size);

  // Now write to the EncryptedSwapFile.
  return EncryptedSwapFile::WriteToSwap(Region(compressed_buf), swap_region);
}

}  // namespace userspace_swap
}  // namespace memory
}  // namespace ash
