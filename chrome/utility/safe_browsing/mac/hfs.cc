// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/safe_browsing/mac/hfs.h"

#include <libkern/OSByteOrder.h>
#include <stddef.h>
#include <sys/stat.h>

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/containers/span.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/numerics/safe_math.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/utility/safe_browsing/mac/convert_big_endian.h"
#include "chrome/utility/safe_browsing/mac/read_stream.h"

namespace safe_browsing {
namespace dmg {

// UTF-16 character for file path seprator.
static const char16_t kFilePathSeparator = u'/';

// We cannot pass pointers to members of packed structs directly to
// ConvertBigEndian, since the alignment of the member may be lower than the
// normally expected alignment of the type, which means an aligned load through
// the pointer could fail.
template <typename T>
static T FromBigEndian(T big_endian_value) {
  T value = big_endian_value;
  ConvertBigEndian(&value);
  return value;
}

static void ConvertBigEndian(HFSPlusForkData* fork) {
  fork->logicalSize = FromBigEndian(fork->logicalSize);
  fork->clumpSize = FromBigEndian(fork->clumpSize);
  fork->totalBlocks = FromBigEndian(fork->totalBlocks);
  for (HFSPlusExtentDescriptor& extent : base::span(fork->extents)) {
    extent.startBlock = FromBigEndian(extent.startBlock);
    extent.blockCount = FromBigEndian(extent.blockCount);
  }
}

static void ConvertBigEndian(HFSPlusVolumeHeader* header) {
  header->signature = FromBigEndian(header->signature);
  header->version = FromBigEndian(header->version);
  header->attributes = FromBigEndian(header->attributes);
  header->lastMountedVersion = FromBigEndian(header->lastMountedVersion);
  header->journalInfoBlock = FromBigEndian(header->journalInfoBlock);
  header->createDate = FromBigEndian(header->createDate);
  header->modifyDate = FromBigEndian(header->modifyDate);
  header->backupDate = FromBigEndian(header->backupDate);
  header->checkedDate = FromBigEndian(header->checkedDate);
  header->fileCount = FromBigEndian(header->fileCount);
  header->folderCount = FromBigEndian(header->folderCount);
  header->blockSize = FromBigEndian(header->blockSize);
  header->totalBlocks = FromBigEndian(header->totalBlocks);
  header->freeBlocks = FromBigEndian(header->freeBlocks);
  header->nextAllocation = FromBigEndian(header->nextAllocation);
  header->rsrcClumpSize = FromBigEndian(header->rsrcClumpSize);
  header->dataClumpSize = FromBigEndian(header->dataClumpSize);
  header->nextCatalogID = FromBigEndian(header->nextCatalogID);
  header->writeCount = FromBigEndian(header->writeCount);
  header->encodingsBitmap = FromBigEndian(header->encodingsBitmap);
  ConvertBigEndian(&header->allocationFile);
  ConvertBigEndian(&header->extentsFile);
  ConvertBigEndian(&header->catalogFile);
  ConvertBigEndian(&header->attributesFile);
  ConvertBigEndian(&header->startupFile);
}

static void ConvertBigEndian(BTHeaderRec* header) {
  header->treeDepth = FromBigEndian(header->treeDepth);
  header->rootNode = FromBigEndian(header->rootNode);
  header->leafRecords = FromBigEndian(header->leafRecords);
  header->firstLeafNode = FromBigEndian(header->firstLeafNode);
  header->lastLeafNode = FromBigEndian(header->lastLeafNode);
  header->nodeSize = FromBigEndian(header->nodeSize);
  header->maxKeyLength = FromBigEndian(header->maxKeyLength);
  header->totalNodes = FromBigEndian(header->totalNodes);
  header->freeNodes = FromBigEndian(header->freeNodes);
  header->reserved1 = FromBigEndian(header->reserved1);
  header->clumpSize = FromBigEndian(header->clumpSize);
  header->attributes = FromBigEndian(header->attributes);
}

static void ConvertBigEndian(BTNodeDescriptor* node) {
  node->fLink = FromBigEndian(node->fLink);
  node->bLink = FromBigEndian(node->bLink);
  node->numRecords = FromBigEndian(node->numRecords);
}

static void ConvertBigEndian(HFSPlusCatalogFolder* folder) {
  folder->recordType = FromBigEndian(folder->recordType);
  folder->flags = FromBigEndian(folder->flags);
  folder->valence = FromBigEndian(folder->valence);
  folder->folderID = FromBigEndian(folder->folderID);
  folder->createDate = FromBigEndian(folder->createDate);
  folder->contentModDate = FromBigEndian(folder->contentModDate);
  folder->attributeModDate = FromBigEndian(folder->attributeModDate);
  folder->accessDate = FromBigEndian(folder->accessDate);
  folder->backupDate = FromBigEndian(folder->backupDate);
  folder->bsdInfo.ownerID = FromBigEndian(folder->bsdInfo.ownerID);
  folder->bsdInfo.groupID = FromBigEndian(folder->bsdInfo.groupID);
  folder->bsdInfo.fileMode = FromBigEndian(folder->bsdInfo.fileMode);
  folder->textEncoding = FromBigEndian(folder->textEncoding);
  folder->folderCount = FromBigEndian(folder->folderCount);
}

static void ConvertBigEndian(HFSPlusCatalogFile* file) {
  file->recordType = FromBigEndian(file->recordType);
  file->flags = FromBigEndian(file->flags);
  file->reserved1 = FromBigEndian(file->reserved1);
  file->fileID = FromBigEndian(file->fileID);
  file->createDate = FromBigEndian(file->createDate);
  file->contentModDate = FromBigEndian(file->contentModDate);
  file->attributeModDate = FromBigEndian(file->attributeModDate);
  file->accessDate = FromBigEndian(file->accessDate);
  file->backupDate = FromBigEndian(file->backupDate);
  file->bsdInfo.ownerID = FromBigEndian(file->bsdInfo.ownerID);
  file->bsdInfo.groupID = FromBigEndian(file->bsdInfo.groupID);
  file->bsdInfo.fileMode = FromBigEndian(file->bsdInfo.fileMode);
  file->userInfo.fdType = FromBigEndian(file->userInfo.fdType);
  file->userInfo.fdCreator = FromBigEndian(file->userInfo.fdCreator);
  file->userInfo.fdFlags = FromBigEndian(file->userInfo.fdFlags);
  file->textEncoding = FromBigEndian(file->textEncoding);
  file->reserved2 = FromBigEndian(file->reserved2);
  ConvertBigEndian(&file->dataFork);
  ConvertBigEndian(&file->resourceFork);
}

// A ReadStream implementation for an HFS+ fork. This only consults the eight
// fork extents. This does not consult the extent overflow file.
class HFSForkReadStream : public ReadStream {
 public:
  HFSForkReadStream(HFSIterator* hfs, const HFSPlusForkData& fork);

  HFSForkReadStream(const HFSForkReadStream&) = delete;
  HFSForkReadStream& operator=(const HFSForkReadStream&) = delete;

  ~HFSForkReadStream() override;

  bool Read(base::span<uint8_t> buf, size_t* bytes_read) override;
  // Seek only supports SEEK_SET.
  off_t Seek(off_t offset, int whence) override;

 private:
  const raw_ptr<HFSIterator> hfs_;  // The HFS+ iterator.
  const HFSPlusForkData fork_;  // The fork to be read.
  base::span<const HFSPlusExtentDescriptor>
      extents_;  // All extents in the fork.
  base::span<const HFSPlusExtentDescriptor>::iterator
      current_extent_;        // The current extent in the fork.
  bool read_current_extent_;  // Whether the current_extent_ has been read.
  std::vector<uint8_t> current_extent_data_;  // Data for |current_extent_|.
  size_t fork_logical_offset_;  // The logical offset into the fork.
};

// HFSBTreeIterator iterates over the HFS+ catalog file.
class HFSBTreeIterator {
 public:
  struct Entry {
    uint16_t record_type;  // Catalog folder item type.
    std::u16string path;   // Full path to the item.
    bool unexported;  // Whether this is HFS+ private data.
    union {
      // This field is not a raw_ptr<> because it was filtered by the rewriter
      // for: #union
      RAW_PTR_EXCLUSION HFSPlusCatalogFile* file;
      // This field is not a raw_ptr<> because it was filtered by the rewriter
      // for: #union
      RAW_PTR_EXCLUSION HFSPlusCatalogFolder* folder;
    };
  };

  HFSBTreeIterator();

  HFSBTreeIterator(const HFSBTreeIterator&) = delete;
  HFSBTreeIterator& operator=(const HFSBTreeIterator&) = delete;

  ~HFSBTreeIterator();

  bool Init(ReadStream* stream);

  bool HasNext();
  bool Next();

  const Entry* current_record() const { return &current_record_; }

 private:
  // Seeks |stream_| to the catalog node ID.
  bool SeekToNode(uint32_t node_id);

  // If required, reads the current leaf into |leaf_data_| and updates the
  // buffer offsets.
  bool ReadCurrentLeaf();

  // Returns a pointer to data at |current_leaf_offset_| in |leaf_data_|. This
  // then advances the offset by the size of the object being returned.
  template <typename T> T* GetLeafData();

  // Checks if the HFS+ catalog key is a Mac OS X reserved key that should not
  // have it or its contents iterated over.
  bool IsKeyUnexported(const std::u16string& path);

  raw_ptr<ReadStream> stream_;  // The stream backing the catalog file.
  BTHeaderRec header_;  // The header B-tree node.

  // Maps CNIDs to their full path. This is used to construct full paths for
  // items that descend from the folders in this map.
  std::map<uint32_t, std::u16string> folder_cnid_map_;

  // CNIDs of the non-exported folders reserved by OS X. If an item has this
  // CNID as a parent, it should be skipped.
  std::set<uint32_t> unexported_parents_;

  // The total number of leaf records read from all the leaf nodes.
  uint32_t leaf_records_read_;

  // The number of records read from the current leaf node.
  uint32_t current_leaf_records_read_;
  uint32_t current_leaf_number_;  // The node ID of the leaf being read.
  // Whether the |current_leaf_number_|'s data has been read into the
  // |leaf_data_| buffer.
  bool read_current_leaf_;
  // The node data for |current_leaf_number_| copied from |stream_|.
  std::vector<uint8_t> leaf_data_;
  size_t current_leaf_offset_;  // The offset in |leaf_data_|.

  // Pointer to |leaf_data_| as a BTNodeDescriptor.
  raw_ptr<const BTNodeDescriptor> current_leaf_;
  Entry current_record_;  // The record read at |current_leaf_offset_|.

  // Constant, string16 versions of the __APPLE_API_PRIVATE values.
  const std::u16string kHFSMetadataFolder{u"\0\0\0\0HFS+ Private Data", 21};
  const std::u16string kHFSDirMetadataFolder =
      u".HFS+ Private Directory Data\r";
};

HFSIterator::HFSIterator(ReadStream* stream)
    : stream_(stream),
      volume_header_() {
}

HFSIterator::~HFSIterator() {}

bool HFSIterator::Open() {
  if (stream_->Seek(1024, SEEK_SET) != 1024)
    return false;

  if (!stream_->ReadType(volume_header_)) {
    DLOG(ERROR) << "Failed to read volume header";
    return false;
  }
  ConvertBigEndian(&volume_header_);

  if (volume_header_.signature != kHFSPlusSigWord &&
      volume_header_.signature != kHFSXSigWord) {
    DLOG(ERROR) << "Unrecognized volume header signature "
                << volume_header_.signature;
    return false;
  }

  if (volume_header_.blockSize == 0) {
    DLOG(ERROR) << "Invalid volume header block size "
                << volume_header_.blockSize;
    return false;
  }

  if (!ReadCatalogFile())
    return false;

  return true;
}

bool HFSIterator::Next() {
  if (!catalog_->HasNext())
    return false;

  // The iterator should only stop on file and folders, skipping over "thread
  // records". In addition, unexported private files and directories should be
  // skipped as well.
  bool keep_going = false;
  do {
    keep_going = catalog_->Next();
    if (keep_going) {
      if (!catalog_->current_record()->unexported &&
          (catalog_->current_record()->record_type == kHFSPlusFolderRecord ||
           catalog_->current_record()->record_type == kHFSPlusFileRecord)) {
        return true;
      }
      keep_going = catalog_->HasNext();
    }
  } while (keep_going);

  return keep_going;
}

bool HFSIterator::IsDirectory() {
  return catalog_->current_record()->record_type == kHFSPlusFolderRecord;
}

bool HFSIterator::IsSymbolicLink() {
  if (IsDirectory())
    return S_ISLNK(catalog_->current_record()->folder->bsdInfo.fileMode);
  else
    return S_ISLNK(catalog_->current_record()->file->bsdInfo.fileMode);
}

bool HFSIterator::IsHardLink() {
  if (IsDirectory())
    return false;
  const HFSPlusCatalogFile* file = catalog_->current_record()->file;
  return file->userInfo.fdType == kHardLinkFileType &&
         file->userInfo.fdCreator == kHFSPlusCreator;
}

bool HFSIterator::IsDecmpfsCompressed() {
  if (IsDirectory())
    return false;
  const HFSPlusCatalogFile* file = catalog_->current_record()->file;
  return file->bsdInfo.ownerFlags & UF_COMPRESSED;
}

std::u16string HFSIterator::GetPath() {
  return catalog_->current_record()->path;
}

std::unique_ptr<ReadStream> HFSIterator::GetReadStream() {
  if (IsDirectory() || IsHardLink())
    return nullptr;

  DCHECK_EQ(kHFSPlusFileRecord, catalog_->current_record()->record_type);
  return std::make_unique<HFSForkReadStream>(
      this, catalog_->current_record()->file->dataFork);
}

bool HFSIterator::SeekToBlock(uint64_t block) {
  uint64_t offset = block * volume_header_.blockSize;
  off_t rv = stream_->Seek(offset, SEEK_SET);
  return rv >= 0 && static_cast<uint64_t>(rv) == offset;
}

bool HFSIterator::ReadCatalogFile() {
  catalog_file_ =
      std::make_unique<HFSForkReadStream>(this, volume_header_.catalogFile);
  catalog_ = std::make_unique<HFSBTreeIterator>();
  return catalog_->Init(catalog_file_.get());
}

HFSForkReadStream::HFSForkReadStream(HFSIterator* hfs,
                                     const HFSPlusForkData& fork)
    : hfs_(hfs),
      fork_(fork),
      extents_(fork.extents),
      current_extent_(extents_.begin()),
      read_current_extent_(false),
      current_extent_data_(),
      fork_logical_offset_(0) {}

HFSForkReadStream::~HFSForkReadStream() {}

bool HFSForkReadStream::Read(base::span<uint8_t> buf, size_t* bytes_read) {
  size_t buffer_space_remaining = buf.size();
  *bytes_read = 0;

  if (fork_logical_offset_ == fork_.logicalSize)
    return true;

  for (; current_extent_ != extents_.end(); ++current_extent_) {
    // If the buffer is out of space, do not attempt any reads. Check this
    // here, so that current_extent_ is advanced by the loop if the last
    // extent was fully read.
    if (buffer_space_remaining == 0)
      break;

    const HFSPlusExtentDescriptor& extent = *current_extent_;

    // A zero-length extent means end-of-fork.
    if (extent.startBlock == 0 && extent.blockCount == 0) {
      break;
    }

    auto extent_size =
        base::CheckedNumeric<size_t>(extent.blockCount) * hfs_->block_size();
    if (extent_size.ValueOrDefault(0) == 0) {
      DLOG(ERROR) << "Extent blockCount overflows or is 0";
      return false;
    }

    // Read the entire extent now, to avoid excessive seeking and re-reading.
    if (!read_current_extent_) {
      if (!hfs_->SeekToBlock(extent.startBlock)) {
        DLOG(ERROR) << "Failed to seek to block " << extent.startBlock;
        return false;
      }
      current_extent_data_.resize(extent_size.ValueOrDie());
      if (!hfs_->stream()->ReadExact(current_extent_data_)) {
        DLOG(ERROR) << "Failed to read extent";
        return false;
      }

      read_current_extent_ = true;
    }

    size_t extent_offset = (fork_logical_offset_ % extent_size).ValueOrDie();
    size_t bytes_to_copy = std::min(
        std::min(
            static_cast<size_t>(fork_.logicalSize) - fork_logical_offset_,
            static_cast<size_t>((extent_size - extent_offset).ValueOrDie())),
        buffer_space_remaining);

    base::span<uint8_t> current_data =
        base::span(current_extent_data_).subspan(extent_offset, bytes_to_copy);
    buf.last(buffer_space_remaining).copy_prefix_from(current_data);

    buffer_space_remaining -= bytes_to_copy;
    *bytes_read += bytes_to_copy;
    fork_logical_offset_ += bytes_to_copy;

    // If the fork's data have been read, then end the loop.
    if (fork_logical_offset_ == fork_.logicalSize)
      return true;

    // If this extent still has data to be copied out, then the read was
    // partial and the buffer is full. Do not advance to the next extent.
    if (extent_offset < current_extent_data_.size())
      break;

    // Advance to the next extent, so reset the state.
    read_current_extent_ = false;
  }

  return true;
}

off_t HFSForkReadStream::Seek(off_t offset, int whence) {
  DCHECK_EQ(SEEK_SET, whence);
  DCHECK_GE(offset, 0);
  DCHECK(offset == 0 || static_cast<uint64_t>(offset) < fork_.logicalSize);
  size_t target_block = offset / hfs_->block_size();
  size_t block_count = 0;
  for (auto it = extents_.begin(); it != extents_.end(); ++it) {
    const HFSPlusExtentDescriptor& extent = *it;

    // An empty extent indicates end-of-fork.
    if (extent.startBlock == 0 && extent.blockCount == 0) {
      break;
    }

    base::CheckedNumeric<size_t> new_block_count(block_count);
    new_block_count += extent.blockCount;
    if (!new_block_count.IsValid()) {
      DLOG(ERROR) << "Seek offset block count overflows";
      return false;
    }

    if (target_block < new_block_count.ValueOrDie()) {
      if (current_extent_ != it) {
        read_current_extent_ = false;
        current_extent_ = it;
      }
      auto iterator_block_offset =
          base::CheckedNumeric<size_t>(block_count) * hfs_->block_size();
      if (!iterator_block_offset.IsValid()) {
        DLOG(ERROR) << "Seek block offset overflows";
        return false;
      }
      fork_logical_offset_ = offset;
      return offset;
    }

    block_count = new_block_count.ValueOrDie();
  }
  return -1;
}

HFSBTreeIterator::HFSBTreeIterator()
    : stream_(),
      header_(),
      leaf_records_read_(0),
      current_leaf_records_read_(0),
      current_leaf_number_(0),
      read_current_leaf_(false),
      leaf_data_(),
      current_leaf_offset_(0),
      current_leaf_() {
}

HFSBTreeIterator::~HFSBTreeIterator() {}

bool HFSBTreeIterator::Init(ReadStream* stream) {
  DCHECK(!stream_);
  stream_ = stream;

  if (stream_->Seek(0, SEEK_SET) != 0) {
    DLOG(ERROR) << "Failed to seek to header node";
    return false;
  }

  BTNodeDescriptor node;
  if (!stream_->ReadType(node)) {
    DLOG(ERROR) << "Failed to read BTNodeDescriptor";
    return false;
  }
  ConvertBigEndian(&node);

  if (node.kind != kBTHeaderNode) {
    DLOG(ERROR) << "Initial node is not a header node";
    return false;
  }

  if (!stream_->ReadType(header_)) {
    DLOG(ERROR) << "Failed to read BTHeaderRec";
    return false;
  }
  ConvertBigEndian(&header_);

  if (header_.nodeSize < sizeof(BTNodeDescriptor)) {
    DLOG(ERROR) << "Invalid header: node size smaller than BTNodeDescriptor";
    return false;
  }

  current_leaf_number_ = header_.firstLeafNode;
  leaf_data_.resize(header_.nodeSize);

  return true;
}

bool HFSBTreeIterator::HasNext() {
  return leaf_records_read_ < header_.leafRecords;
}

bool HFSBTreeIterator::Next() {
  if (!ReadCurrentLeaf())
    return false;

  GetLeafData<uint16_t>();  // keyLength

  uint32_t parent_id;
  if (auto* parent_id_ptr = GetLeafData<uint32_t>()) {
    parent_id = OSSwapBigToHostInt32(*parent_id_ptr);
  } else {
    return false;
  }

  uint16_t key_string_length;
  if (auto* key_string_length_ptr = GetLeafData<uint16_t>()) {
    key_string_length = OSSwapBigToHostInt16(*key_string_length_ptr);
  } else {
    return false;
  }

  // Read and byte-swap the variable-length key string.
  std::u16string key(key_string_length, '\0');
  for (uint16_t i = 0; i < key_string_length; ++i) {
    auto* character = GetLeafData<uint16_t>();
    if (!character) {
      DLOG(ERROR) << "Key string length points past leaf data";
      return false;
    }
    key[i] = OSSwapBigToHostInt16(*character);
  }

  // Read the record type and then rewind as the field is part of the catalog
  // structure that is read next.
  auto* record_type = GetLeafData<int16_t>();
  if (!record_type) {
    DLOG(ERROR) << "Failed to read record type";
    return false;
  }
  current_record_.record_type = OSSwapBigToHostInt16(*record_type);
  current_record_.unexported = false;
  current_leaf_offset_ -= sizeof(int16_t);
  switch (current_record_.record_type) {
    case kHFSPlusFolderRecord: {
      auto* folder = GetLeafData<HFSPlusCatalogFolder>();
      ConvertBigEndian(folder);
      ++leaf_records_read_;
      ++current_leaf_records_read_;

      // If this key is unexported, or the parent folder is, then mark the
      // record as such.
      if (IsKeyUnexported(key) ||
          unexported_parents_.find(parent_id) != unexported_parents_.end()) {
        unexported_parents_.insert(folder->folderID);
        current_record_.unexported = true;
      }

      // Update the CNID map to construct the path tree.
      if (parent_id != 0) {
        auto parent_name = folder_cnid_map_.find(parent_id);
        if (parent_name != folder_cnid_map_.end())
          key = parent_name->second + kFilePathSeparator + key;
      }
      folder_cnid_map_[folder->folderID] = key;

      current_record_.path = key;
      current_record_.folder = folder;
      break;
    }
    case kHFSPlusFileRecord: {
      auto* file = GetLeafData<HFSPlusCatalogFile>();
      ConvertBigEndian(file);
      ++leaf_records_read_;
      ++current_leaf_records_read_;

      std::u16string path =
          folder_cnid_map_[parent_id] + kFilePathSeparator + key;
      current_record_.path = path;
      current_record_.file = file;
      current_record_.unexported =
          unexported_parents_.find(parent_id) != unexported_parents_.end();
      break;
    }
    case kHFSPlusFolderThreadRecord:
    case kHFSPlusFileThreadRecord: {
      // Thread records are used to quickly locate a file or folder just by
      // CNID. As these are not necessary for the iterator, skip past the data.
      GetLeafData<uint16_t>();  // recordType
      GetLeafData<uint16_t>();  // reserved
      GetLeafData<uint32_t>();  // parentID
      auto string_length = OSSwapBigToHostInt16(*GetLeafData<uint16_t>());
      for (uint16_t i = 0; i < string_length; ++i)
        GetLeafData<uint16_t>();
      ++leaf_records_read_;
      ++current_leaf_records_read_;
      break;
    }
    default:
      DLOG(ERROR) << "Unknown record type " << current_record_.record_type;
      return false;
  }

  // If all the records from this leaf have been read, follow the forward link
  // to the next B-Tree leaf node.
  if (current_leaf_records_read_ >= current_leaf_->numRecords) {
    current_leaf_number_ = current_leaf_->fLink;
    read_current_leaf_ = false;
  }

  return true;
}

bool HFSBTreeIterator::SeekToNode(uint32_t node_id) {
  if (node_id >= header_.totalNodes)
    return false;
  size_t offset = node_id * header_.nodeSize;
  if (stream_->Seek(offset, SEEK_SET) != -1) {
    current_leaf_number_ = node_id;
    return true;
  }
  return false;
}

bool HFSBTreeIterator::ReadCurrentLeaf() {
  if (read_current_leaf_)
    return true;

  if (!SeekToNode(current_leaf_number_)) {
    DLOG(ERROR) << "Failed to seek to node " << current_leaf_number_;
    return false;
  }

  CHECK_EQ(leaf_data_.size(), header_.nodeSize);
  if (!stream_->ReadExact(leaf_data_)) {
    DLOG(ERROR) << "Failed to read node " << current_leaf_number_;
    return false;
  }

  auto* leaf = reinterpret_cast<BTNodeDescriptor*>(leaf_data_.data());
  ConvertBigEndian(leaf);
  if (leaf->kind != kBTLeafNode) {
    DLOG(ERROR) << "Node " << current_leaf_number_ << " is not a leaf";
    return false;
  }
  current_leaf_ = leaf;
  current_leaf_offset_ = sizeof(BTNodeDescriptor);
  current_leaf_records_read_ = 0;
  read_current_leaf_ = true;
  return true;
}

template <typename T>
T* HFSBTreeIterator::GetLeafData() {
  base::CheckedNumeric<size_t> size = sizeof(T);
  auto new_offset = size + current_leaf_offset_;
  if (!new_offset.IsValid() || new_offset.ValueOrDie() >= leaf_data_.size())
    return nullptr;
  T* object = reinterpret_cast<T*>(&leaf_data_[current_leaf_offset_]);
  current_leaf_offset_ = new_offset.ValueOrDie();
  return object;
}

bool HFSBTreeIterator::IsKeyUnexported(const std::u16string& key) {
  return key == kHFSDirMetadataFolder ||
         key == kHFSMetadataFolder;
}

}  // namespace dmg
}  // namespace safe_browsing
