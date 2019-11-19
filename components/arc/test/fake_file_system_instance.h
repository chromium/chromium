// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_TEST_FAKE_FILE_SYSTEM_INSTANCE_H_
#define COMPONENTS_ARC_TEST_FAKE_FILE_SYSTEM_INSTANCE_H_

#include <stdint.h>

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "components/arc/mojom/file_system.mojom.h"
#include "storage/browser/file_system/watcher_manager.h"

namespace arc {

// Fake implementation to operate on documents in memory.
//
// ArcFileSystemOperationRunner provides two types of methods: content URL
// based and documents provider based. According to backend type, you need
// to setup the fake with different functions.
//
// Content URL based functions are:
// - GetFileSize()
// - GetMimeType()
// - OpenFileToRead()
// - OpenFileToWrite()
// Fake files for those functions can be set up by AddFile().
//
// Documents provider based functions are:
// - GetDocument()
// - GetChildDocuments()
// - GetRecentDocuments()
// - GetRoots()
// Fake documents for those functions can be set up by AddDocument() and fake
// roots for those functions can be set up by AddRoot().
//
// Notes:
// - GetChildDocuments() returns child documents in the same order as they were
//   added with AddDocument().
// - GetRecentDocuments() returns recent documents in the same order as they
//   were added with AddRecentDocument().
// - OpenFileToRead() and OpenFileToWrite() will fail unless AddFile() for the
//   file to open is called beforehand.
// - Callbacks are never invoked synchronously.
// - All member functions must be called on the same thread.
class FakeFileSystemInstance : public mojom::FileSystemInstance {
 public:
  // Specification of a fake file available to content URL based methods.
  struct File {
    enum class Seekable {
      NO,
      YES,
    };

    // Content URL of a file.
    std::string url;

    // The content of a file, which can be read by OpenFileToRead().
    // When Seekable.NO is specified and OpenFileToWrite() is called, this
    // |content| will be ignored and bytes written to FD from OpenFileToWrite()
    // will be read by OpenFileToRead().
    std::string content;

    // The MIME type of a file.
    std::string mime_type;

    // Whether this file is seekable or not.
    Seekable seekable;

    File(const std::string& url,
         const std::string& content,
         const std::string& mime_type,
         Seekable seekable);
    File(const File& that);
    ~File();
  };

  // Specification of a fake document available to documents provider based
  // methods.
  struct Document {
    // Authority.
    std::string authority;

    // ID of this document.
    std::string document_id;

    // ID of the parent document. Can be empty if this is a root.
    std::string parent_document_id;

    // File name displayed to users.
    std::string display_name;

    // MIME type.
    std::string mime_type;

    // File size in bytes. Set to -1 if size is not available.
    int64_t size;

    // Last modified time in milliseconds from the UNIX epoch.
    // TODO(crbug.com/672737): Use base::Time once the corresponding field
    // in file_system.mojom stops using uint64.
    uint64_t last_modified;

    // Flag indicating that a document is deletable.
    bool supports_delete;

    // Flag indicating that a document can be renamed.
    bool supports_rename;

    // Flag indicating that a document is a directory that supports creation of
    // new files within it.
    bool dir_supports_create;

    Document(const std::string& authority,
             const std::string& document_id,
             const std::string& parent_document_id,
             const std::string& display_name,
             const std::string& mime_type,
             int64_t size,
             uint64_t last_modified);
    Document(const std::string& authority,
             const std::string& document_id,
             const std::string& parent_document_id,
             const std::string& display_name,
             const std::string& mime_type,
             int64_t size,
             uint64_t last_modified,
             bool supports_delete,
             bool supports_rename,
             bool dir_supports_create);
    Document(const Document& that);
    ~Document();
  };

  // Specification of a fake root available to documents provider based methods.
  struct Root {
    // Authority.
    std::string authority;

    // ID of this root.
    std::string root_id;

    // ID of the document which is a directory that represents the top
    // directory of this root.
    std::string document_id;

    // Title of this root.
    std::string title;

    Root(const std::string& authority,
         const std::string& root_id,
         const std::string& document_id,
         const std::string& title);
    Root(const Root& that);
    ~Root();
  };

  FakeFileSystemInstance();
  ~FakeFileSystemInstance() override;

  // Returns true if Init() has been called.
  bool InitCalled();

  // Adds a file accessible by content URL based methods.
  void AddFile(const File& file);

  // Adds a document accessible by document provider based methods.
  void AddDocument(const Document& document);

  // Adds a recent document accessible by document provider based methods.
  void AddRecentDocument(const std::string& root_id, const Document& document);

  // Adds a root accessible by document provider based methods.
  void AddRoot(const Root& root);

  // Triggers watchers installed to a document.
  void TriggerWatchers(const std::string& authority,
                       const std::string& document_id,
                       storage::WatcherManager::ChangeType type);

  // Returns how many times GetChildDocuments() was called.
  int get_child_documents_count() const { return get_child_documents_count_; }

  // Returns true if there is a document with the given authority and
  // document_id.
  bool DocumentExists(const std::string& authority,
                      const std::string& document_id);

  // Returns true if there is a document with the given authority,
  // root's document_id, and file path from the root.
  bool DocumentExists(const std::string& authority,
                      const std::string& root_document_id,
                      const base::FilePath& path);

  // Returns a document with the given authority and document_id.
  Document GetDocument(const std::string& authority,
                       const std::string& document_id);

  // Returns a document with the given authority, root's document_id, and file
  // path from the root.
  Document GetDocument(const std::string& authority,
                       const std::string& document_id,
                       const base::FilePath& path);

  // Returns the content written to the FD returned by OpenFileToWrite().
  std::string GetFileContent(const std::string& url);

  // Returns the content written to the FD returned by OpenFileToWrite(), up to
  // |bytes| bytes.
  std::string GetFileContent(const std::string& url, size_t bytes);

  // For clearing the list of handled requests
  void clear_handled_requests() { handled_url_requests_.clear(); }

  // For validating the url requests in testing.
  const std::vector<mojom::OpenUrlsRequestPtr>& handledUrlRequests() const {
    return handled_url_requests_;
  }

  // mojom::FileSystemInstance:
  void AddWatcher(const std::string& authority,
                  const std::string& document_id,
                  AddWatcherCallback callback) override;
  void GetChildDocuments(const std::string& authority,
                         const std::string& document_id,
                         GetChildDocumentsCallback callback) override;
  void GetDocument(const std::string& authority,
                   const std::string& document_id,
                   GetDocumentCallback callback) override;
  void GetFileSize(const std::string& url,
                   GetFileSizeCallback callback) override;
  void GetMimeType(const std::string& url,
                   GetMimeTypeCallback callback) override;
  void GetRecentDocuments(const std::string& authority,
                          const std::string& root_id,
                          GetRecentDocumentsCallback callback) override;
  void GetRoots(GetRootsCallback callback) override;
  void DeleteDocument(const std::string& authority,
                      const std::string& document_id,
                      DeleteDocumentCallback callback) override;
  void RenameDocument(const std::string& authority,
                      const std::string& document_id,
                      const std::string& display_name,
                      RenameDocumentCallback callback) override;
  void CreateDocument(const std::string& authority,
                      const std::string& parent_document_id,
                      const std::string& mime_type,
                      const std::string& display_name,
                      CreateDocumentCallback callback) override;
  void CopyDocument(const std::string& authority,
                    const std::string& source_document_id,
                    const std::string& target_parent_document_id,
                    CopyDocumentCallback callback) override;
  void MoveDocument(const std::string& authority,
                    const std::string& source_document_id,
                    const std::string& source_parent_document_id,
                    const std::string& target_parent_document_id,
                    MoveDocumentCallback callback) override;
  void InitDeprecated(mojom::FileSystemHostPtr host) override;
  void Init(mojom::FileSystemHostPtr host, InitCallback callback) override;
  void OpenFileToRead(const std::string& url,
                      OpenFileToReadCallback callback) override;
  void OpenFileToWrite(const std::string& url,
                       OpenFileToWriteCallback callback) override;
  void RemoveWatcher(int64_t watcher_id,
                     RemoveWatcherCallback callback) override;
  void RequestMediaScan(const std::vector<std::string>& paths) override;
  void OpenUrlsWithPermission(mojom::OpenUrlsRequestPtr request,
                              OpenUrlsWithPermissionCallback callback) override;

 private:
  // A pair of an authority and a document ID which identifies the location
  // of a document in documents providers.
  using DocumentKey = std::pair<std::string, std::string>;

  // A pair of an authority and a root ID which identifies a root in
  // documents providers.
  using RootKey = std::pair<std::string, std::string>;

  // Finds a document inside a document with parent_document_id by following
  // given path components.
  std::string FindChildDocumentId(const std::string& authority,
                                  const std::string& parent_document_id,
                                  const std::vector<std::string>& components);

  // Creates a regular file with the given flags and returns its fd.
  base::ScopedFD CreateRegularFileDescriptor(const File& file, uint32_t flags);

  // Returns a FD to non-seekable file to read. |content| content will be
  // returned when reading the FD.
  base::ScopedFD CreateStreamFileDescriptorToRead(const std::string& content);

  // Returns a FD to non-seekable file to write. Bytes written to the FD can be
  // read by calling GetFileContent() with the passed |url|.
  base::ScopedFD CreateStreamFileDescriptorToWrite(const std::string& url);

  THREAD_CHECKER(thread_checker_);

  base::ScopedTempDir temp_dir_;

  mojom::FileSystemHostPtr host_;

  // Mapping from a content URL to a file.
  std::map<std::string, File> files_;

  // Mapping from a content URL to a file path of a created regular file.
  std::map<std::string, base::FilePath> regular_file_paths_;

  // Mapping from a content URL to a read end of a pipe.
  // The corresponding write end should have been returned from
  // OpenFileToWrite() on non-seekable file entry.
  std::map<std::string, base::ScopedFD> pipe_read_ends_;

  // Mapping from a document key to a document.
  std::map<DocumentKey, Document> documents_;

  // Mapping from a document key to its child documents.
  std::map<DocumentKey, std::vector<DocumentKey>> child_documents_;

  // Mapping from a root to its recent documents.
  std::map<RootKey, std::vector<Document>> recent_documents_;

  // Mapping from a document key to its watchers.
  std::map<DocumentKey, std::set<int64_t>> document_to_watchers_;

  // Mapping from a watcher ID to a document key.
  std::map<int64_t, DocumentKey> watcher_to_document_;

  // List of all OpenUrlsRequests made to the fake_file_system_instance
  std::vector<mojom::OpenUrlsRequestPtr> handled_url_requests_;

  // List of roots added by AddRoot().
  std::vector<Root> roots_;

  int64_t next_watcher_id_ = 1;
  int get_child_documents_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(FakeFileSystemInstance);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_TEST_FAKE_FILE_SYSTEM_INSTANCE_H_
