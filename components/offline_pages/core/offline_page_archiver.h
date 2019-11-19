// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_ARCHIVER_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_ARCHIVER_H_

#include <cstdint>
#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/strings/string16.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "components/offline_pages/core/offline_page_types.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

namespace offline_pages {

// Interface of a class responsible for creation of the archive for offline use.
//
// Archiver will be implemented by embedder and may have additional methods that
// are not interesting from the perspective of OfflinePageModel. Example of such
// extra information or capability is a way to enumerate available WebContents
// to find the one that needs to be used to create archive (or to map it to the
// URL passed in CreateArchive in some other way).
//
// Archiver will be responsible for naming the file that is being saved (it has
// URL, title and the whole page content at its disposal). For that it should be
// also configured with the path where the archives are stored.
//
// Archiver should be able to archive multiple pages in parallel, as these are
// asynchronous calls carried out by some other component.
//
// If archiver gets two consecutive requests to archive the same page (may be
// run in parallel) it can generate 2 different names for files and save the
// same page separately, as if these were 2 completely unrelated pages. It is up
// to the caller (e.g. OfflinePageModel) to make sure that situation like that
// does not happen.
//
// If the page is not completely loaded, it is up to the implementation of the
// archiver whether to respond with ERROR_CONTENT_UNAVAILABLE, wait longer to
// actually snapshot a complete page, or snapshot whatever is available at that
// point in time (what the user sees).
class OfflinePageArchiver {
 public:
  // Results of the archive creation.
  enum class ArchiverResult {
    SUCCESSFULLY_CREATED,           // Archive created successfully.
    ERROR_DEVICE_FULL,              // Cannot save the archive - device is full.
    ERROR_CANCELED,                 // Caller canceled the request.
    ERROR_CONTENT_UNAVAILABLE,      // Content to archive is not available.
    ERROR_ARCHIVE_CREATION_FAILED,  // Creation of archive failed.
    ERROR_SKIPPED,                  // Page shouldn't be archived like NTP or
                                    // file urls.
    ERROR_DIGEST_CALCULATION_FAILED,  // Failed to compute digest.
  };

  // Describes the parameters to control how to create an archive.
  struct CreateArchiveParams {
    explicit CreateArchiveParams(const std::string& name_space);

    // The offline page namespace associated with the archive to be created.
    std::string name_space;

    // Whether to remove popup overlay that obstructs viewing normal content.
    bool remove_popup_overlay = false;

    // Run page problem detectors while generating MTHML if true.
    bool use_page_problem_detectors = false;

    // Whether to enable on-the-fly hash computation.
    bool use_on_the_fly_hash_computation = false;
  };

  // Callback for the final result of an attempt to generate of offline page
  // archive. All parameters after |result| are only set in the case of a
  // successful archive creation.
  using CreateArchiveCallback =
      base::OnceCallback<void(ArchiverResult /* result */,
                              const GURL& /* url */,
                              const base::FilePath& /* file_path */,
                              const base::string16& /* title */,
                              int64_t /* file_size */,
                              const std::string& /* digest */)>;

  virtual ~OfflinePageArchiver() {}

  // Starts creating the archive in the |archives_dir| per
  // |create_archive_params|. Once archive is created |callback| will be called
  // with the result and additional information.
  virtual void CreateArchive(const base::FilePath& archives_dir,
                             const CreateArchiveParams& create_archive_params,
                             content::WebContents* web_contents,
                             CreateArchiveCallback callback) = 0;

};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_ARCHIVER_H_
