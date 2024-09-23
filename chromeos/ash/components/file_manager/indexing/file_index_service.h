// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_FILE_MANAGER_INDEXING_FILE_INDEX_SERVICE_H_
#define CHROMEOS_ASH_COMPONENTS_FILE_MANAGER_INDEXING_FILE_INDEX_SERVICE_H_

#include <vector>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/threading/sequence_bound.h"
#include "chromeos/ash/components/file_manager/indexing/file_index.h"
#include "chromeos/ash/components/file_manager/indexing/file_info.h"
#include "chromeos/ash/components/file_manager/indexing/query.h"
#include "chromeos/ash/components/file_manager/indexing/term.h"
#include "url/gurl.h"

namespace ash::file_manager {

// The type of callback on which search results are reported.
typedef base::OnceCallback<void(SearchResults)> SearchResultsCallback;

// The type of callback on which operations that manipulate terms, files or
// initialize the index are reported.
typedef base::OnceCallback<void(OpResults)> IndexingOperationCallback;

// A file indexing service. The main task of this service is to efficiently
// associate terms with files. Instead of using files directly, we rely on
// the FileInfo class, which stores file's URL, size and modification time.
// Terms are pairs of field:text, where field identifies where the text is
// coming from. For example, if text is derived from the files content, the
// field can be "content". if the text is a label added to the file, the field
// could be "label".
//
// A typical use of the index is to register file via the PutFileInfo() method
// followed by a call SetTerms() for files, which creates association between
// terms and passed file info. Later, those files can be efficiently retrieved
// by calling the Search() method and passing a query to it. If the underlying
// file is removed from the file system, the RemoveFile() method can be called
// with the URL of the file to purge it from the index.
//
// FileIndexService* service = FileIndexServiceFactory::GetForBrowserContext(
//    context);
// service->PutFileInfo(pinned_file_info,
//                      base::BindOnce([](OpResults results) {
//                        if (results != OpResults::kSuccess) { ... }
//                      }));
// service->PutFileInfo(downloaded_file_info,
//                      base::BindOnce([](OpResults results) {
//                        if (results != OpResults::kSuccess) { ... }
//                      }));
// service->SetTerms({Term("label", "pinned")},
//                    pinned_file_info.file_url,
//                    base::BindOnce([](OpResults results) {
//                      if (results != OpResults::kSuccess) { ... }
//                    }));
// service->AddTerms({Term("label", "downloaded")},
//                    downloaded_file_info.file_url,
//                    base::BindOnce([](OpResults results) {
//                      if (results != OpResults::kSuccess) { ... }
//                    }));
// ...
// std::vector<FileInfo> downloaded_files = service->Search(
//     Query({Term("label", "downloaded")},
//           base::BindOnce([](SearchResults results) {
//             ... // display results
//           })));
class COMPONENT_EXPORT(FILE_MANAGER) FileIndexService {
 public:
  explicit FileIndexService(base::FilePath profile_path);
  ~FileIndexService();

  FileIndexService(const FileIndexService&) = delete;
  FileIndexService& operator=(const FileIndexService&) = delete;

  // Initializes this service; must be called before the service is used.
  void Init(IndexingOperationCallback callback);

  // Registers the given file info with this index. This operation must be
  // completed before terms can be added to or removed from the file with
  // the matching URL.
  void PutFileInfo(const FileInfo& info, IndexingOperationCallback callback);

  // Removes the file uniquely identified by the URL from this index. This is
  // preferred way of removing files over calling the SetTerms method with an
  // empty terms vector. Returns true if the file was found and removed.
  void RemoveFile(const GURL& url, IndexingOperationCallback callback);

  // Moves the the file that was put under the `old_url` to be associated with
  // the `new_url`. If the file with the `old_url` is not found, the callback
  // is called with kFileMissing error. If the association with the `new_url`
  // fails the callback is called with kGenericError. Otherwise, it is called
  // with kSuccess.
  void MoveFile(const GURL& old_url,
                const GURL& new_url,
                IndexingOperationCallback callback);

  // Sets terms associated with the file. You may not set an empty set of terms
  // on the file. Please note that only the passed terms remains associated with
  // the file. Thus if you call this method first with, say,
  // Term("label", "downloaded"), and then call this method with,
  // say, Term("label", "pinned") only the "pinned" label is associated with
  // the given `file_info`. If you want both terms to be associated you must
  // pass both terms in a single call or use the AddTerms() method.
  void SetTerms(const std::vector<Term>& terms,
                const GURL& url,
                IndexingOperationCallback callback);

  // Adds terms associated with the file with the `terms` given as the first
  // argument. Once this operation is finished, the file can be retrieved by any
  // existing terms that were associated with it, or any new terms this call
  // added.
  void AddTerms(const std::vector<Term>& terms,
                const GURL& url,
                IndexingOperationCallback callback);

  // Removes the specified terms from list of terms associated with the given
  // `url`.
  void RemoveTerms(const std::vector<Term>& terms,
                   const GURL& url,
                   IndexingOperationCallback callback);

  // Searches the index for file info matching the specified query.
  void Search(const Query& query, SearchResultsCallback callback);

 private:
  // A fully synchronous file index that handles the asynchronous calls.
  base::SequenceBound<FileIndex> file_index_;

  // Remembers if init was called to prevent multiple calls.
  OpResults inited_ = OpResults::kUndefined;
};

}  // namespace ash::file_manager

#endif  // CHROMEOS_ASH_COMPONENTS_FILE_MANAGER_INDEXING_FILE_INDEX_SERVICE_H_
