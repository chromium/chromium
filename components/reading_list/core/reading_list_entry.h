// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_READING_LIST_CORE_READING_LIST_ENTRY_H_
#define COMPONENTS_READING_LIST_CORE_READING_LIST_ENTRY_H_

#include <string>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "net/base/backoff_entry.h"
#include "url/gurl.h"

namespace reading_list {

class ReadingListLocal;

// The different ways a reading list entry is added.
// |ADDED_VIA_CURRENT_APP| is when the entry was added by the user from within
// the current instance of the app.
// |ADDED_VIA_EXTENSION| is when the entry was added via the share extension.
// |ADDED_VIA_SYNC| is when the entry was added with sync.
enum EntrySource { ADDED_VIA_CURRENT_APP, ADDED_VIA_EXTENSION, ADDED_VIA_SYNC };

}  // namespace reading_list

namespace sync_pb {
class ReadingListSpecifics;
}  // namespace sync_pb

class ReadingListEntry;

// An entry in the reading list. The URL is a unique identifier for an entry, as
// such it should not be empty and is the only thing considered when comparing
// entries.
// A word about timestamp usage in this class:
// - The backing store uses int64 values to code timestamps. We use internally
//   the same type to avoid useless conversions. This values represent the
//   number of micro seconds since Jan 1st 1970.
// - As most timestamp are used to sort entries, operations on int64_t are
//   faster than operations on base::Time. So Getter return the int64_t values.
// - However, to ensure all the conversions are done the same way, and because
//   the Now time is alway retrieved using base::Time::Now(), all the timestamp
//   parameter are passed as base::Time. These parameters are internally
//   converted in int64_t.
class ReadingListEntry : public base::RefCounted<ReadingListEntry> {
 public:
  // Creates a ReadingList entry. |url| and |title| are the main fields of the
  // entry.
  // |now| is used to fill the |creation_time_us_| and all the update timestamp
  // fields.
  ReadingListEntry(const GURL& url,
                   const std::string& title,
                   const base::Time& now);
  ReadingListEntry(const GURL& url,
                   const std::string& title,
                   const base::Time& now,
                   std::unique_ptr<net::BackoffEntry> backoff);

  ReadingListEntry(ReadingListEntry&& entry) = delete;
  ReadingListEntry& operator=(ReadingListEntry&&) = delete;
  ReadingListEntry(const ReadingListEntry&) = delete;
  ReadingListEntry& operator=(const ReadingListEntry&) = delete;

  // Entries are created in WAITING state. At some point they will be PROCESSING
  // into one of the three state: PROCESSED, the only state a distilled URL
  // would be set, WILL_RETRY, similar to wait, but with exponential delays or
  // DISTILLATION_ERROR where the system will not retry at all.
  enum DistillationState {
    WAITING,
    PROCESSING,
    PROCESSED,
    WILL_RETRY,
    DISTILLATION_ERROR
  };

  static const net::BackoffEntry::Policy kBackoffPolicy;

  // The URL of the page the user would like to read later.
  const GURL& URL() const;
  // The title of the entry. Might be empty.
  const std::string& Title() const;
  // The estimated time to read of the entry. Zero if none available.
  base::TimeDelta EstimatedReadTime() const;
  // What state this entry is in.
  DistillationState DistilledState() const;
  // The local file path for the distilled version of the page. This should only
  // be called if the state is "PROCESSED".
  const base::FilePath& DistilledPath() const;
  // The URL that has been distilled to produce file stored at |DistilledPath|.
  const GURL& DistilledURL() const;
  // The time distillation was done. The value is in microseconds since Jan 1st
  // 1970. Returns 0 if the entry was not distilled.
  int64_t DistillationTime() const;
  // The size of the stored page in bytes.
  // TODO(crbug.com/40894644): Remove after M115
  int64_t DistillationSize() const;
  // The time before the next try. This is automatically increased when the
  // state is set to WILL_RETRY or ERROR from a non-error state.
  base::TimeDelta TimeUntilNextTry() const;
  // The number of time chrome failed to download this entry. This is
  // automatically increased when the state is set to WILL_RETRY or ERROR from a
  // non-error state.
  int FailedDownloadCounter() const;
  // The read status of the entry.
  bool IsRead() const;
  // Returns if an entry has ever been seen.
  bool HasBeenSeen() const;
  // Returns whether the passed ReadingListSpecifics can be used to construct an
  // entry via FromReadingListValidSpecifics().
  static bool IsSpecificsValid(const sync_pb::ReadingListSpecifics& pb_entry);

  // The last update time of the entry. This value may be used to sort the
  // entries. The value is in microseconds since Jan 1st 1970.
  int64_t UpdateTime() const;

  // The last update time of the title of the entry. The value is in
  // microseconds since Jan 1st 1970.
  int64_t UpdateTitleTime() const;

  // The creation update time of the entry. The value is in microseconds since
  // Jan 1st 1970.
  int64_t CreationTime() const;

  // The time when the entry was read for the first time. The value is in
  // microseconds since Jan 1st 1970.
  int64_t FirstReadTime() const;

  // Set the update time to |now|.
  void MarkEntryUpdated(const base::Time& now);

  // Returns a protobuf encoding the content of this ReadingListEntry for local
  // storage. Use |now| to serialize the backoff_entry.
  std::unique_ptr<reading_list::ReadingListLocal> AsReadingListLocal(
      const base::Time& now) const;

  // Returns a protobuf encoding the content of this ReadingListEntry for sync.
  std::unique_ptr<sync_pb::ReadingListSpecifics> AsReadingListSpecifics() const;

  // Created a ReadingListEntry from the protobuf format.
  // Use |now| to deserialize the backoff_entry.
  static scoped_refptr<ReadingListEntry> FromReadingListLocal(
      const reading_list::ReadingListLocal& pb_entry,
      const base::Time& now);

  // Created a ReadingListEntry from the protobuf format.
  // If creation time is not set, it will be set to |now|.
  // Please note that |pb_entry| must be valid, as per IsSpecificsValid().
  static scoped_refptr<ReadingListEntry> FromReadingListValidSpecifics(
      const sync_pb::ReadingListSpecifics& pb_entry,
      const base::Time& now);

  // Merge |this| and |other| into this.
  // Local fields are kept from |this|.
  // Each field is merged individually keeping the highest value as defined by
  // the |ReadingListSyncBridge.CompareEntriesForSync| function.
  //
  // After calling |MergeLocalStateFrom|, the result must verify
  // ReadingListSyncBridge::CompareEntriesForSync(
  //     old_this.AsReadingListSpecifics(),
  //     new_this.AsReadingListSpecifics())
  // and
  // ReadingListSyncBridge::CompareEntriesForSync(
  //     other.AsReadingListSpecifics(),
  //     new_this.AsReadingListSpecifics()).
  void MergeWithEntry(const ReadingListEntry& other);

  scoped_refptr<ReadingListEntry> Clone() const;

  bool operator==(const ReadingListEntry& other) const;

  // Sets |title_| to |title|. Sets |update_title_time_us_| to |now|.
  void SetTitle(const std::string& title, const base::Time& now);
  // Sets the distilled info (offline path, online URL, size and date of the
  // stored files) about distilled page, switch the state to PROCESSED and reset
  // the time until the next try.
  void SetDistilledInfo(const base::FilePath& path,
                        const GURL& distilled_url,
                        int64_t distilation_size,
                        const base::Time& distilation_time);
  // Sets the state to one of PROCESSING, WILL_RETRY or ERROR.
  void SetDistilledState(DistillationState distilled_state);
  // Sets the read state of the entry. Will set the UpdateTime of the entry.
  // If |first_read_time_us_| is 0 and read is READ, sets |first_read_time_us_|
  // to |now|.
  void SetRead(bool read, const base::Time& now);
  // Sets the estimated time to read of this entry page.
  void SetEstimatedReadTime(base::TimeDelta estimated_read_time);

 private:
  friend class base::RefCounted<ReadingListEntry>;

  enum State { UNSEEN, UNREAD, READ };
  ReadingListEntry(const GURL& url,
                   const std::string& title,
                   base::TimeDelta estimated_read_time,
                   State state,
                   int64_t creation_time,
                   int64_t first_read_time,
                   int64_t update_time,
                   int64_t update_title_time,
                   ReadingListEntry::DistillationState distilled_state,
                   const base::FilePath& distilled_path,
                   const GURL& distilled_url,
                   int64_t distillation_time,
                   int64_t distillation_size,
                   int failed_download_counter,
                   std::unique_ptr<net::BackoffEntry> backoff);

  ~ReadingListEntry();

  GURL url_;
  std::string title_;
  base::TimeDelta estimated_read_time_;
  State state_;
  base::FilePath distilled_path_;
  GURL distilled_url_;
  DistillationState distilled_state_;

  std::unique_ptr<net::BackoffEntry> backoff_;
  int failed_download_counter_;

  // These value are in microseconds since Jan 1st 1970. They are used for
  // sorting the entries from the database. They are kept in int64_t to avoid
  // conversion on each save/read event.
  int64_t creation_time_us_;
  int64_t first_read_time_us_;
  int64_t update_time_us_;
  int64_t update_title_time_us_;
  int64_t distillation_time_us_;
  // TODO(crbug.com/40894644): Remove after M115
  int64_t distillation_size_;
};

#endif  // COMPONENTS_READING_LIST_CORE_READING_LIST_ENTRY_H_
