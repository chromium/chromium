// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// This file contains the implementation for an iterator over a portable
// executable file's resources.

#include "chrome/installer/test/pe_image_resources.h"

#include "base/logging.h"
#include "base/win/pe_image.h"

namespace {

// Performs a cast to type |T| of |data| iff |data_size| is sufficient to hold
// an instance of type |T|.  Returns true on success.
template <class T>
bool StructureAt(const uint8_t* data, size_t data_size, const T** structure) {
  if (sizeof(T) <= data_size) {
    *structure = reinterpret_cast<const T*>(data);
    return true;
  }
  return false;
}

// Recursive function for enumerating entries in an image's resource segment.
// static
bool EnumResourcesWorker(const base::win::PEImage& image,
                         const uint8_t* tree_base,
                         DWORD tree_size,
                         DWORD directory_offset,
                         upgrade_test::EntryPath* path,
                         upgrade_test::EnumResource_Fn callback,
                         uintptr_t context) {
  bool success = true;
  const IMAGE_RESOURCE_DIRECTORY* resource_directory;

  if (!StructureAt(tree_base + directory_offset, tree_size - directory_offset,
                   &resource_directory) ||
      directory_offset + sizeof(IMAGE_RESOURCE_DIRECTORY) +
              (resource_directory->NumberOfNamedEntries +
               resource_directory->NumberOfIdEntries) *
                  sizeof(IMAGE_RESOURCE_DIRECTORY_ENTRY) >
          tree_size) {
    LOG(DFATAL) << "Insufficient room in resource segment for directory entry.";
    return false;
  }

  const IMAGE_RESOURCE_DIRECTORY_ENTRY* scan =
      reinterpret_cast<const IMAGE_RESOURCE_DIRECTORY_ENTRY*>(
          tree_base + directory_offset + sizeof(IMAGE_RESOURCE_DIRECTORY));
  const IMAGE_RESOURCE_DIRECTORY_ENTRY* end =
      scan + resource_directory->NumberOfNamedEntries +
      resource_directory->NumberOfIdEntries;
  for (; success && scan != end; ++scan) {
    if ((scan->NameIsString != 0) !=
        (scan - reinterpret_cast<const IMAGE_RESOURCE_DIRECTORY_ENTRY*>(
                    tree_base + directory_offset +
                    sizeof(IMAGE_RESOURCE_DIRECTORY)) <
         resource_directory->NumberOfNamedEntries)) {
      LOG(DFATAL) << "Inconsistent number of named or numbered entries.";
      success = false;
      break;
    }
    if (scan->NameIsString) {
      const IMAGE_RESOURCE_DIR_STRING_U* dir_string;
      if (!StructureAt(tree_base + scan->NameOffset,
                       tree_size - scan->NameOffset, &dir_string) ||
          scan->NameOffset + sizeof(WORD) +
                  dir_string->Length * sizeof(wchar_t) >
              tree_size) {
        LOG(DFATAL) << "Insufficient room in resource segment for entry name.";
        success = false;
        break;
      }
      path->push_back(upgrade_test::EntryId(
          std::wstring(&dir_string->NameString[0], dir_string->Length)));
    } else {
      path->push_back(upgrade_test::EntryId(scan->Id));
    }
    if (scan->DataIsDirectory) {
      success =
          EnumResourcesWorker(image, tree_base, tree_size,
                              scan->OffsetToDirectory, path, callback, context);
    } else {
      const IMAGE_RESOURCE_DATA_ENTRY* data_entry;
      if (StructureAt(tree_base + scan->OffsetToData,
                      tree_size - scan->OffsetToData, &data_entry) &&
          reinterpret_cast<uint8_t*>(
              image.RVAToAddr(data_entry->OffsetToData)) +
                  data_entry->Size <=
              tree_base + tree_size) {
        // Despite what winnt.h says, OffsetToData is an RVA.
        callback(*path,
                 reinterpret_cast<uint8_t*>(
                     image.RVAToAddr(data_entry->OffsetToData)),
                 data_entry->Size, data_entry->CodePage, context);
      } else {
        LOG(DFATAL) << "Insufficient room in resource segment for data entry.";
        success = false;
      }
    }
    path->pop_back();
  }

  return success;
}

}  // namespace

namespace upgrade_test {

// static
bool EnumResources(const base::win::PEImage& image,
                   EnumResource_Fn callback,
                   uintptr_t context) {
  DWORD resources_size =
      image.GetImageDirectoryEntrySize(IMAGE_DIRECTORY_ENTRY_RESOURCE);
  if (resources_size != 0) {
    EntryPath path_storage;
    return EnumResourcesWorker(
        image,
        reinterpret_cast<uint8_t*>(
            image.GetImageDirectoryEntryAddr(IMAGE_DIRECTORY_ENTRY_RESOURCE)),
        resources_size, 0, &path_storage, callback, context);
  }
  return true;
}

}  // namespace upgrade_test
