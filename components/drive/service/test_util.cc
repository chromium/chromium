// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/drive/service/test_util.h"

#include <memory>
#include <string>

#include "base/run_loop.h"
#include "components/drive/drive_api_util.h"
#include "components/drive/service/fake_drive_service.h"
#include "google_apis/common/test_util.h"
#include "google_apis/drive/drive_api_parser.h"

using google_apis::ApiErrorCode;
using google_apis::FileResource;
using google_apis::HTTP_CREATED;
using google_apis::OTHER_ERROR;

namespace drive {
namespace test_util {

bool SetUpTestEntries(FakeDriveService* drive_service) {
  return SetUpTestEntries(drive_service, drive_service->GetRootResourceId());
}

bool SetUpTeamDriveTestEntries(FakeDriveService* drive_service,
                               const std::string& team_drive_id,
                               const std::string& team_drive_name) {
  drive_service->AddTeamDrive(team_drive_id, team_drive_name);
  return SetUpTestEntries(drive_service, team_drive_id);
}

bool SetUpTestEntries(FakeDriveService* drive_service,
                      const std::string& parent_resource_id) {
  ApiErrorCode error = OTHER_ERROR;
  std::unique_ptr<FileResource> entry;

  drive_service->AddNewFileWithResourceId(
      "2_file_resource_id", "audio/mpeg", "This is some test content.",
      parent_resource_id, "File 1.txt",
      false,  // shared_with_me
      google_apis::test_util::CreateCopyResultCallback(&error, &entry));
  base::RunLoop().RunUntilIdle();
  if (error != HTTP_CREATED)
    return false;

  drive_service->AddNewFileWithResourceId(
      "slash_file_resource_id", "audio/mpeg", "This is some test content.",
      parent_resource_id, "Slash / in file 1.txt",
      false,  // shared_with_me
      google_apis::test_util::CreateCopyResultCallback(&error, &entry));
  base::RunLoop().RunUntilIdle();
  if (error != HTTP_CREATED)
    return false;

  drive_service->AddNewFileWithResourceId(
      "3_file_resource_id", "audio/mpeg", "This is some test content.",
      parent_resource_id, "Duplicate Name.txt",
      false,  // shared_with_me
      google_apis::test_util::CreateCopyResultCallback(&error, &entry));
  base::RunLoop().RunUntilIdle();
  if (error != HTTP_CREATED)
    return false;

  drive_service->AddNewFileWithResourceId(
      "4_file_resource_id", "audio/mpeg", "This is some test content.",
      parent_resource_id, "Duplicate Name.txt",
      false,  // shared_with_me
      google_apis::test_util::CreateCopyResultCallback(&error, &entry));
  base::RunLoop().RunUntilIdle();
  if (error != HTTP_CREATED)
    return false;

  drive_service->AddNewFileWithResourceId(
      "5_document_resource_id", util::kGoogleDocumentMimeType, std::string(),
      parent_resource_id, "Document 1 excludeDir-test",
      false,  // shared_with_me
      google_apis::test_util::CreateCopyResultCallback(&error, &entry));
  base::RunLoop().RunUntilIdle();
  if (error != HTTP_CREATED)
    return false;

  drive_service->AddNewFileWithResourceId(
      "1_folder_resource_id", util::kDriveFolderMimeType, std::string(),
      parent_resource_id, "Directory 1",
      false,  // shared_with_me
      google_apis::test_util::CreateCopyResultCallback(&error, &entry));
  base::RunLoop().RunUntilIdle();
  if (error != HTTP_CREATED)
    return false;

  drive_service->AddNewFileWithResourceId(
      "subdirectory_file_1_id", "audio/mpeg", "This is some test content.",
      "1_folder_resource_id", "SubDirectory File 1.txt",
      false,  // shared_with_me
      google_apis::test_util::CreateCopyResultCallback(&error, &entry));
  base::RunLoop().RunUntilIdle();
  if (error != HTTP_CREATED)
    return false;

  drive_service->AddNewFileWithResourceId(
      "subdirectory_unowned_file_1_id", "audio/mpeg",
      "This is some test content.", "1_folder_resource_id",
      "Shared to The Account Owner.txt",
      true,  // shared_with_me
      google_apis::test_util::CreateCopyResultCallback(&error, &entry));
  base::RunLoop().RunUntilIdle();
  if (error != HTTP_CREATED)
    return false;

  drive_service->AddNewDirectoryWithResourceId(
      "sub_dir_folder_resource_id", "1_folder_resource_id",
      "Sub Directory Folder", AddNewDirectoryOptions(),
      google_apis::test_util::CreateCopyResultCallback(&error, &entry));
  base::RunLoop().RunUntilIdle();
  if (error != HTTP_CREATED)
    return false;

  drive_service->AddNewDirectoryWithResourceId(
      "sub_sub_directory_folder_id", "sub_dir_folder_resource_id",
      "Sub Sub Directory Folder", AddNewDirectoryOptions(),
      google_apis::test_util::CreateCopyResultCallback(&error, &entry));
  base::RunLoop().RunUntilIdle();
  if (error != HTTP_CREATED)
    return false;

  drive_service->AddNewDirectoryWithResourceId(
      "slash_dir_folder_resource_id", parent_resource_id,
      "Slash / in directory", AddNewDirectoryOptions(),
      google_apis::test_util::CreateCopyResultCallback(&error, &entry));
  base::RunLoop().RunUntilIdle();
  if (error != HTTP_CREATED)
    return false;

  drive_service->AddNewFileWithResourceId(
      "slash_subdir_file", "audio/mpeg", "This is some test content.",
      "slash_dir_folder_resource_id", "Slash SubDir File.txt",
      false,  // shared_with_me
      google_apis::test_util::CreateCopyResultCallback(&error, &entry));
  base::RunLoop().RunUntilIdle();
  if (error != HTTP_CREATED)
    return false;

  drive_service->AddNewDirectoryWithResourceId(
      "sub_dir_folder_2_self_link", parent_resource_id,
      "Directory 2 excludeDir-test", AddNewDirectoryOptions(),
      google_apis::test_util::CreateCopyResultCallback(&error, &entry));
  base::RunLoop().RunUntilIdle();
  if (error != HTTP_CREATED)
    return false;

  drive_service->AddNewFileWithResourceId(
      "1_orphanfile_resource_id", "text/plain", "This is some test content.",
      std::string(), "Orphan File 1.txt",
      true,  // shared_with_me
      google_apis::test_util::CreateCopyResultCallback(&error, &entry));
  base::RunLoop().RunUntilIdle();
  if (error != HTTP_CREATED)
    return false;

  drive_service->AddNewFileWithResourceId(
      "orphan_doc_1", util::kGoogleDocumentMimeType, std::string(),
      std::string(), "Orphan Document",
      true,  // shared_with_me
      google_apis::test_util::CreateCopyResultCallback(&error, &entry));
  base::RunLoop().RunUntilIdle();
  if (error != HTTP_CREATED)
    return false;

  return true;
}

}  // namespace test_util
}  // namespace drive
