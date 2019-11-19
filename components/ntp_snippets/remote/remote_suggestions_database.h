// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_REMOTE_REMOTE_SUGGESTIONS_DATABASE_H_
#define COMPONENTS_NTP_SNIPPETS_REMOTE_REMOTE_SUGGESTIONS_DATABASE_H_

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/ntp_snippets/remote/remote_suggestion.h"

namespace base {
class FilePath;
}  // namespace base

namespace leveldb_proto {
class ProtoDatabaseProvider;
}  // namespace leveldb_proto

namespace ntp_snippets {

class SnippetImageProto;
class SnippetProto;

// TODO(gaschler): implement a Fake version for testing
class RemoteSuggestionsDatabase {
 public:
  using SnippetsCallback =
      base::OnceCallback<void(RemoteSuggestion::PtrVector)>;
  using SnippetImageCallback = base::OnceCallback<void(std::string)>;

  // Creates a RemoteSuggestionsDatabase backed by real ProtoDatabases.
  RemoteSuggestionsDatabase(
      leveldb_proto::ProtoDatabaseProvider* proto_database_provider,
      const base::FilePath& database_dir);
  // Creates a RemoteSuggestionsDatabase backed by the passed-in ProtoDatabases,
  // useful for testing.
  RemoteSuggestionsDatabase(
      std::unique_ptr<leveldb_proto::ProtoDatabase<SnippetProto>> database,
      std::unique_ptr<leveldb_proto::ProtoDatabase<SnippetImageProto>>
          image_database);
  ~RemoteSuggestionsDatabase();

  // Returns whether the database has finished initialization. While this is
  // false, loads may already be started (they'll be serviced after
  // initialization finishes), but no updates are allowed.
  bool IsInitialized() const;

  // Returns whether the database is in an (unrecoverable) error state. If this
  // is true, the database must not be used anymore
  bool IsErrorState() const;

  // Set a callback to be called when the database enters an error state.
  void SetErrorCallback(const base::Closure& error_callback);

  // Loads all snippets from storage and passes them to |callback|.
  void LoadSnippets(SnippetsCallback callback);

  // Adds or updates the given snippet.
  void SaveSnippet(const RemoteSuggestion& snippet);
  // Adds or updates all the given snippets.
  void SaveSnippets(const RemoteSuggestion::PtrVector& snippets);

  // Deletes the snippet with the given ID.
  void DeleteSnippet(const std::string& snippet_id);
  // Deletes all the given snippets (identified by their IDs).
  void DeleteSnippets(std::unique_ptr<std::vector<std::string>> snippet_ids);

  // Loads the image data for the snippet with the given ID and passes it to
  // |callback|. Passes an empty string if not found.
  void LoadImage(const std::string& snippet_id, SnippetImageCallback callback);

  // Adds or updates the image data for the given snippet ID.
  void SaveImage(const std::string& snippet_id, const std::string& image_data);

  // Deletes the image data for the given snippet ID.
  void DeleteImage(const std::string& snippet_id);
  // Deletes the image data for the given snippets (identified by their IDs).
  void DeleteImages(std::unique_ptr<std::vector<std::string>> snippet_ids);
  // Deletes all images which are not associated with any of the provided
  // snippets.
  void GarbageCollectImages(
      std::unique_ptr<std::set<std::string>> alive_snippet_ids);

 private:
  friend class RemoteSuggestionsDatabaseTest;

  using KeyEntryVector =
      leveldb_proto::ProtoDatabase<SnippetProto>::KeyEntryVector;

  using ImageKeyEntryVector =
      leveldb_proto::ProtoDatabase<SnippetImageProto>::KeyEntryVector;

  RemoteSuggestionsDatabase(
      leveldb_proto::ProtoDatabaseProvider* proto_database_provider,
      const base::FilePath& database_dir,
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  // Callbacks for ProtoDatabase<SnippetProto> operations.
  void OnDatabaseInited(leveldb_proto::Enums::InitStatus status);
  void OnDatabaseLoaded(SnippetsCallback callback,
                        bool success,
                        std::unique_ptr<std::vector<SnippetProto>> entries);
  void OnDatabaseSaved(bool success);

  // Callbacks for ProtoDatabase<SnippetImageProto> operations.
  void OnImageDatabaseInited(leveldb_proto::Enums::InitStatus status);
  void OnImageDatabaseLoaded(SnippetImageCallback callback,
                             bool success,
                             std::unique_ptr<SnippetImageProto> entry);
  void OnImageDatabaseSaved(bool success);

  void OnDatabaseError();

  void ProcessPendingLoads();

  void LoadSnippetsImpl(SnippetsCallback callback);
  void SaveSnippetsImpl(std::unique_ptr<KeyEntryVector> entries_to_save);

  void LoadImageImpl(const std::string& snippet_id,
                     SnippetImageCallback callback);
  void DeleteUnreferencedImages(
      std::unique_ptr<std::set<std::string>> references,
      bool load_keys_success,
      std::unique_ptr<std::vector<std::string>> image_keys);

  std::unique_ptr<leveldb_proto::ProtoDatabase<SnippetProto>> database_;
  bool database_initialized_;
  std::vector<SnippetsCallback> pending_snippets_callbacks_;

  std::unique_ptr<leveldb_proto::ProtoDatabase<SnippetImageProto>>
      image_database_;
  bool image_database_initialized_;
  std::vector<std::pair<std::string, SnippetImageCallback>>
      pending_image_callbacks_;

  base::Closure error_callback_;

  base::WeakPtrFactory<RemoteSuggestionsDatabase> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(RemoteSuggestionsDatabase);
};

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_REMOTE_REMOTE_SUGGESTIONS_DATABASE_H_
