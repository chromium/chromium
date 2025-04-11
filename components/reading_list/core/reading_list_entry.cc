// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reading_list/core/reading_list_entry.h"

#include <memory>

#include "base/json/json_string_value_serializer.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "components/reading_list/core/offline_url_utils.h"
#include "components/reading_list/core/proto/reading_list.pb.h"
#include "components/reading_list/core/reading_list_sync_bridge.h"
#include "components/sync/protocol/reading_list_specifics.pb.h"
#include "net/base/backoff_entry_serializer.h"

namespace {
// Converts |time| to the number of microseconds since Jan 1st 1970.
int64_t TimeToUS(const base::Time& time) {
  return (time - base::Time::UnixEpoch()).InMicroseconds();
}
}  // namespace

// The backoff time is the following: 10min, 10min, 1h, 2h, 2h..., starting
// after the first failure.
const net::BackoffEntry::Policy ReadingListEntry::kBackoffPolicy = {
    // Number of initial errors (in sequence) to ignore before applying
    // exponential back-off rules.
    2,

    // Initial delay for exponential back-off in ms.
    10 * 60 * 1000,  // 10 minutes.

    // Factor by which the waiting time will be multiplied.
    6,

    // Fuzzing percentage. ex: 10% will spread requests randomly
    // between 90%-100% of the calculated time.
    0.1,  // 10%.

    // Maximum amount of time we are willing to delay our request in ms.
    2 * 3600 * 1000,  // 2 hours.

    // Time to keep an entry from being discarded even when it
    // has no significant state, -1 to never discard.
    -1,

    true,  // Don't use initial delay unless the last request was an error.
};

ReadingListEntry::ReadingListEntry(const GURL& url,
                                   const std::string& title,
                                   const base::Time& now)
    : ReadingListEntry(url, title, now, nullptr) {}

ReadingListEntry::ReadingListEntry(const GURL& url,
                                   const std::string& title,
                                   const base::Time& now,
                                   std::unique_ptr<net::BackoffEntry> backoff)
    : ReadingListEntry(url,
                       title,
                       base::TimeDelta(),
                       UNSEEN,
                       TimeToUS(now),
                       0,
                       TimeToUS(now),
                       TimeToUS(now),
                       WAITING,
                       base::FilePath(),
                       GURL(),
                       0,
                       0,
                       0,
                       std::move(backoff)) {}

ReadingListEntry::ReadingListEntry(
    const GURL& url,
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
    std::unique_ptr<net::BackoffEntry> backoff)
    : url_(url),
      title_(title),
      estimated_read_time_(estimated_read_time),
      state_(state),
      distilled_path_(distilled_path),
      distilled_url_(distilled_url),
      distilled_state_(distilled_state),
      failed_download_counter_(failed_download_counter),
      creation_time_us_(creation_time),
      first_read_time_us_(first_read_time),
      update_time_us_(update_time),
      update_title_time_us_(update_title_time),
      distillation_time_us_(distillation_time),
      distillation_size_(distillation_size) {
  if (backoff) {
    backoff_ = std::move(backoff);
  } else {
    backoff_ = std::make_unique<net::BackoffEntry>(&kBackoffPolicy);
  }
  DCHECK(creation_time_us_);
  DCHECK(update_time_us_);
  DCHECK(update_title_time_us_);
  DCHECK(!url.is_empty());
  DCHECK(url.is_valid());
}

ReadingListEntry::~ReadingListEntry() = default;

const GURL& ReadingListEntry::URL() const {
  return url_;
}

const std::string& ReadingListEntry::Title() const {
  return title_;
}

base::TimeDelta ReadingListEntry::EstimatedReadTime() const {
  return estimated_read_time_;
}

ReadingListEntry::DistillationState ReadingListEntry::DistilledState() const {
  return distilled_state_;
}

const base::FilePath& ReadingListEntry::DistilledPath() const {
  return distilled_path_;
}

const GURL& ReadingListEntry::DistilledURL() const {
  return distilled_url_;
}

int64_t ReadingListEntry::DistillationTime() const {
  return distillation_time_us_;
}

int64_t ReadingListEntry::DistillationSize() const {
  return distillation_size_;
}

base::TimeDelta ReadingListEntry::TimeUntilNextTry() const {
  return backoff_->GetTimeUntilRelease();
}

int ReadingListEntry::FailedDownloadCounter() const {
  return failed_download_counter_;
}

scoped_refptr<ReadingListEntry> ReadingListEntry::Clone() const {
  const base::Time now = base::Time::Now();
  scoped_refptr<ReadingListEntry> entry_copy =
      FromReadingListLocal(*AsReadingListLocal(now), now);
  DCHECK(entry_copy->url_ == url_);
  DCHECK(entry_copy->title_ == title_);
  DCHECK(entry_copy->estimated_read_time_ == estimated_read_time_);
  DCHECK(entry_copy->state_ == state_);
  DCHECK(entry_copy->creation_time_us_ == creation_time_us_);
  DCHECK(entry_copy->first_read_time_us_ == first_read_time_us_);
  DCHECK(entry_copy->update_time_us_ == update_time_us_);
  DCHECK(entry_copy->update_title_time_us_ == update_title_time_us_);
  DCHECK(entry_copy->distilled_state_ == distilled_state_);
  DCHECK(entry_copy->distilled_path_ == distilled_path_);
  DCHECK(entry_copy->distilled_url_ == distilled_url_);
  DCHECK(entry_copy->distillation_time_us_ == distillation_time_us_);
  DCHECK(entry_copy->distillation_size_ == distillation_size_);
  DCHECK(entry_copy->failed_download_counter_ == failed_download_counter_);
  return entry_copy;
}

bool ReadingListEntry::operator==(const ReadingListEntry& other) const {
  return url_ == other.url_;
}

void ReadingListEntry::SetTitle(const std::string& title,
                                const base::Time& now) {
  title_ = title;
  update_title_time_us_ = TimeToUS(now);
}

void ReadingListEntry::SetRead(bool read, const base::Time& now) {
  State previous_state = state_;
  state_ = read ? READ : UNREAD;
  if (state_ == previous_state) {
    return;
  }
  if (FirstReadTime() == 0 && read) {
    int64_t time_to_us = TimeToUS(now);
    int64_t time_since_creation =
        (time_to_us - creation_time_us_) / base::Time::kMicrosecondsPerHour;
    base::UmaHistogramCounts1000("ReadingList.Read.AgeOnFirstRead",
                                 time_since_creation);
    first_read_time_us_ = time_to_us;
  }
  if (!(previous_state == UNSEEN && state_ == UNREAD)) {
    // If changing UNSEEN -> UNREAD, entry is not marked updated to preserve
    // order in Reading List View.
    MarkEntryUpdated(now);
  }
}

bool ReadingListEntry::IsRead() const {
  return state_ == READ;
}

bool ReadingListEntry::HasBeenSeen() const {
  return state_ != UNSEEN;
}

bool ReadingListEntry::IsSpecificsValid(
    const sync_pb::ReadingListSpecifics& pb_entry) {
  if (!pb_entry.has_entry_id() || !pb_entry.has_url() ||
      pb_entry.entry_id() != pb_entry.url()) {
    return false;
  }
  GURL url(pb_entry.url());
  if (url.is_empty() || !url.is_valid()) {
    return false;
  }
  // Some crash reports indicate that some users have reading list entries with
  // invalid (non-UTF8) titles, so filter out such invalid items.
  if (!base::IsStringUTF8AllowingNoncharacters(pb_entry.title())) {
    return false;
  }
  return true;
}

void ReadingListEntry::SetEstimatedReadTime(
    base::TimeDelta estimated_read_time) {
  estimated_read_time_ = estimated_read_time;
}

void ReadingListEntry::SetDistilledInfo(const base::FilePath& path,
                                        const GURL& distilled_url,
                                        int64_t distilation_size,
                                        const base::Time& distilation_time) {
  DCHECK(!path.empty());
  DCHECK(distilled_url.is_valid());
  distilled_path_ = path;
  distilled_state_ = PROCESSED;
  distilled_url_ = distilled_url;
  distillation_time_us_ = TimeToUS(distilation_time);
  distillation_size_ = distilation_size;
  backoff_->Reset();
  failed_download_counter_ = 0;
}

void ReadingListEntry::SetDistilledState(DistillationState distilled_state) {
  DCHECK(distilled_state != PROCESSED);  // use SetDistilledPath instead.
  DCHECK(distilled_state != WAITING);
  // Increase time until next retry exponentially if the state change from a
  // non-error state to an error state.
  if ((distilled_state == WILL_RETRY ||
       distilled_state == DISTILLATION_ERROR) &&
      distilled_state_ != WILL_RETRY &&
      distilled_state_ != DISTILLATION_ERROR) {
    backoff_->InformOfRequest(false);
    failed_download_counter_++;
  }

  distilled_state_ = distilled_state;
  distilled_path_ = base::FilePath();
  distilled_url_ = GURL();
  distillation_size_ = 0;
  distillation_time_us_ = 0;
}

int64_t ReadingListEntry::UpdateTime() const {
  return update_time_us_;
}

int64_t ReadingListEntry::UpdateTitleTime() const {
  return update_title_time_us_;
}

int64_t ReadingListEntry::CreationTime() const {
  return creation_time_us_;
}

int64_t ReadingListEntry::FirstReadTime() const {
  return first_read_time_us_;
}

void ReadingListEntry::MarkEntryUpdated(const base::Time& now) {
  update_time_us_ = TimeToUS(now);
}

// static
scoped_refptr<ReadingListEntry> ReadingListEntry::FromReadingListLocal(
    const reading_list::ReadingListLocal& pb_entry,
    const base::Time& now) {
  if (!pb_entry.has_url()) {
    return nullptr;
  }
  GURL url(pb_entry.url());
  if (url.is_empty() || !url.is_valid()) {
    return nullptr;
  }
  std::string title;
  if (pb_entry.has_title()) {
    title = pb_entry.title();
  }

  int64_t creation_time_us = 0;
  if (pb_entry.has_creation_time_us()) {
    creation_time_us = pb_entry.creation_time_us();
  } else {
    creation_time_us = (now - base::Time::UnixEpoch()).InMicroseconds();
  }

  int64_t first_read_time_us = 0;
  if (pb_entry.has_first_read_time_us()) {
    first_read_time_us = pb_entry.first_read_time_us();
  }

  int64_t update_time_us = creation_time_us;
  if (pb_entry.has_update_time_us()) {
    update_time_us = pb_entry.update_time_us();
  }

  int64_t update_title_time_us = 0;
  if (pb_entry.has_update_title_time_us()) {
    update_title_time_us = pb_entry.update_title_time_us();
  }
  if (update_title_time_us == 0) {
    // Entries created before title could be modified don't have
    // update_title_time_us. Set it to creation_time_us for consistency.
    update_title_time_us = creation_time_us;
  }
  base::TimeDelta estimated_read_time;
  if (pb_entry.estimated_read_time_seconds()) {
    estimated_read_time = base::Seconds(pb_entry.estimated_read_time_seconds());
  }

  State state = UNSEEN;
  if (pb_entry.has_status()) {
    switch (pb_entry.status()) {
      case reading_list::ReadingListLocal::READ:
        state = READ;
        break;
      case reading_list::ReadingListLocal::UNREAD:
        state = UNREAD;
        break;
      case reading_list::ReadingListLocal::UNSEEN:
        state = UNSEEN;
        break;
    }
  }

  ReadingListEntry::DistillationState distillation_state =
      ReadingListEntry::WAITING;
  if (pb_entry.has_distillation_state()) {
    switch (pb_entry.distillation_state()) {
      case reading_list::ReadingListLocal::WAITING:
        distillation_state = ReadingListEntry::WAITING;
        break;
      case reading_list::ReadingListLocal::PROCESSING:
        distillation_state = ReadingListEntry::PROCESSING;
        break;
      case reading_list::ReadingListLocal::PROCESSED:
        distillation_state = ReadingListEntry::PROCESSED;
        break;
      case reading_list::ReadingListLocal::WILL_RETRY:
        distillation_state = ReadingListEntry::WILL_RETRY;
        break;
      case reading_list::ReadingListLocal::DISTILLATION_ERROR:
        distillation_state = ReadingListEntry::DISTILLATION_ERROR;
        break;
    }
  }

  base::FilePath distilled_path;
  if (pb_entry.has_distilled_path()) {
    distilled_path = base::FilePath::FromUTF8Unsafe(pb_entry.distilled_path());
  }

  GURL distilled_url;
  if (pb_entry.has_distilled_url()) {
    distilled_url = GURL(pb_entry.distilled_url());
  }

  int64_t distillation_time_us = 0;
  if (pb_entry.has_distillation_time_us()) {
    distillation_time_us = pb_entry.distillation_time_us();
  }

  int64_t distillation_size = 0;
  if (pb_entry.has_distillation_size()) {
    distillation_size = pb_entry.distillation_size();
  }

  int64_t failed_download_counter = 0;
  if (pb_entry.has_failed_download_counter()) {
    failed_download_counter = pb_entry.failed_download_counter();
  }

  std::unique_ptr<net::BackoffEntry> backoff;
  if (pb_entry.has_backoff()) {
    JSONStringValueDeserializer deserializer(pb_entry.backoff());
    std::unique_ptr<base::Value> value(
        deserializer.Deserialize(nullptr, nullptr));
    if (value) {
      DCHECK(value->is_list());
      backoff = net::BackoffEntrySerializer::DeserializeFromList(
          value->GetList(), &kBackoffPolicy, nullptr, now);
    }
  }

  // MakeRefCounted cannot be used because this constructor is private.
  return base::WrapRefCounted<ReadingListEntry>(new ReadingListEntry(
      url, title, estimated_read_time, state, creation_time_us,
      first_read_time_us, update_time_us, update_title_time_us,
      distillation_state, distilled_path, distilled_url, distillation_time_us,
      distillation_size, failed_download_counter, std::move(backoff)));
}

// static
scoped_refptr<ReadingListEntry> ReadingListEntry::FromReadingListValidSpecifics(
    const sync_pb::ReadingListSpecifics& pb_entry,
    const base::Time& now) {
  CHECK(IsSpecificsValid(pb_entry));

  std::string title;
  if (pb_entry.has_title()) {
    title = pb_entry.title();
  }

  int64_t creation_time_us = TimeToUS(now);
  if (pb_entry.has_creation_time_us()) {
    creation_time_us = pb_entry.creation_time_us();
  }

  int64_t first_read_time_us = 0;
  if (pb_entry.has_first_read_time_us()) {
    first_read_time_us = pb_entry.first_read_time_us();
  }

  int64_t update_time_us = creation_time_us;
  if (pb_entry.has_update_time_us()) {
    update_time_us = pb_entry.update_time_us();
  }

  int64_t update_title_time_us = 0;
  if (pb_entry.has_update_title_time_us()) {
    update_title_time_us = pb_entry.update_title_time_us();
  }
  if (update_title_time_us == 0) {
    // Entries created before title could be modified don't have
    // update_title_time_us. Set it to creation_time_us for consistency.
    update_title_time_us = creation_time_us;
  }
  base::TimeDelta estimated_read_time;
  if (pb_entry.has_estimated_read_time_seconds()) {
    estimated_read_time = base::Seconds(pb_entry.estimated_read_time_seconds());
  }

  State state = UNSEEN;
  if (pb_entry.has_status()) {
    switch (pb_entry.status()) {
      case sync_pb::ReadingListSpecifics::READ:
        state = READ;
        break;
      case sync_pb::ReadingListSpecifics::UNREAD:
        state = UNREAD;
        break;
      case sync_pb::ReadingListSpecifics::UNSEEN:
        state = UNSEEN;
        break;
    }
  }

  return base::WrapRefCounted<ReadingListEntry>(new ReadingListEntry(
      GURL(pb_entry.url()), title, estimated_read_time, state, creation_time_us,
      first_read_time_us, update_time_us, update_title_time_us, WAITING,
      base::FilePath(), GURL(), 0, 0, 0, nullptr));
}

void ReadingListEntry::MergeWithEntry(const ReadingListEntry& other) {
#if !defined(NDEBUG)
  // Checks that the result entry respects the sync order.
  std::unique_ptr<sync_pb::ReadingListSpecifics> old_this_pb(
      AsReadingListSpecifics());
  std::unique_ptr<sync_pb::ReadingListSpecifics> other_pb(
      other.AsReadingListSpecifics());
#endif
  DCHECK(url_ == other.url_);
  if (update_title_time_us_ < other.update_title_time_us_) {
    // Take the most recent title updated.
    title_ = std::move(other.title_);
    update_title_time_us_ = std::move(other.update_title_time_us_);
  } else if (update_title_time_us_ == other.update_title_time_us_) {
    if (title_.compare(other.title_) < 0) {
      // Take the last in alphabetical order or the longer one.
      // This ensure empty string is replaced.
      title_ = std::move(other.title_);
    }
  }
  if (creation_time_us_ < other.creation_time_us_) {
    creation_time_us_ = std::move(other.creation_time_us_);
    first_read_time_us_ = std::move(other.first_read_time_us_);
    if (estimated_read_time_ != other.estimated_read_time_ &&
        !other.estimated_read_time_.is_zero()) {
      // Assume that if |other| is newer that its estimated read time is the
      // preferred one.
      estimated_read_time_ = std::move(other.estimated_read_time_);
    }
  } else if (creation_time_us_ == other.creation_time_us_) {
    // The first_time_read_us from |other| is used if
    // - this.first_time_read_us == 0: the entry was never read in this device.
    // - this.first_time_read_us > other.first_time_read_us: the entry was
    //       first read on another device.
    if (first_read_time_us_ == 0 ||
        (other.first_read_time_us_ != 0 &&
         other.first_read_time_us_ < first_read_time_us_)) {
      first_read_time_us_ = std::move(other.first_read_time_us_);
    }
  }
  if (update_time_us_ < other.update_time_us_) {
    update_time_us_ = std::move(other.update_time_us_);
    state_ = std::move(other.state_);
  } else if (update_time_us_ == other.update_time_us_) {
    if (state_ == UNSEEN) {
      state_ = std::move(other.state_);
    } else if (other.state_ == READ) {
      state_ = std::move(other.state_);
    }
  }
#if !defined(NDEBUG)
  std::unique_ptr<sync_pb::ReadingListSpecifics> new_this_pb(
      AsReadingListSpecifics());
  DCHECK(
      ReadingListSyncBridge::CompareEntriesForSync(*old_this_pb, *new_this_pb));
  DCHECK(ReadingListSyncBridge::CompareEntriesForSync(*other_pb, *new_this_pb));
#endif
}

std::unique_ptr<reading_list::ReadingListLocal>
ReadingListEntry::AsReadingListLocal(const base::Time& now) const {
  std::unique_ptr<reading_list::ReadingListLocal> pb_entry =
      std::make_unique<reading_list::ReadingListLocal>();

  // URL is used as the key for the database and sync as there is only one entry
  // per URL.
  pb_entry->set_entry_id(URL().spec());
  pb_entry->set_title(Title());
  pb_entry->set_url(URL().spec());
  pb_entry->set_creation_time_us(CreationTime());
  pb_entry->set_first_read_time_us(FirstReadTime());
  pb_entry->set_update_time_us(UpdateTime());
  pb_entry->set_update_title_time_us(UpdateTitleTime());
  pb_entry->set_estimated_read_time_seconds(EstimatedReadTime().InSeconds());

  switch (state_) {
    case READ:
      pb_entry->set_status(reading_list::ReadingListLocal::READ);
      break;
    case UNREAD:
      pb_entry->set_status(reading_list::ReadingListLocal::UNREAD);
      break;
    case UNSEEN:
      pb_entry->set_status(reading_list::ReadingListLocal::UNSEEN);
      break;
  }

  reading_list::ReadingListLocal::DistillationState distilation_state =
      reading_list::ReadingListLocal::WAITING;
  switch (DistilledState()) {
    case ReadingListEntry::WAITING:
      distilation_state = reading_list::ReadingListLocal::WAITING;
      break;
    case ReadingListEntry::PROCESSING:
      distilation_state = reading_list::ReadingListLocal::PROCESSING;
      break;
    case ReadingListEntry::PROCESSED:
      distilation_state = reading_list::ReadingListLocal::PROCESSED;
      break;
    case ReadingListEntry::WILL_RETRY:
      distilation_state = reading_list::ReadingListLocal::WILL_RETRY;
      break;
    case ReadingListEntry::DISTILLATION_ERROR:
      distilation_state = reading_list::ReadingListLocal::DISTILLATION_ERROR;
      break;
  }
  pb_entry->set_distillation_state(distilation_state);
  if (!DistilledPath().empty()) {
    pb_entry->set_distilled_path(DistilledPath().AsUTF8Unsafe());
  }
  if (DistilledURL().is_valid()) {
    pb_entry->set_distilled_url(DistilledURL().spec());
  }
  if (DistillationTime()) {
    pb_entry->set_distillation_time_us(DistillationTime());
  }
  if (DistillationSize()) {
    pb_entry->set_distillation_size(DistillationSize());
  }

  pb_entry->set_failed_download_counter(failed_download_counter_);

  if (backoff_) {
    base::Value::List backoff =
        net::BackoffEntrySerializer::SerializeToList(*backoff_, now);

    std::string output;
    JSONStringValueSerializer serializer(&output);
    serializer.Serialize(backoff);
    pb_entry->set_backoff(output);
  }

  return pb_entry;
}

std::unique_ptr<sync_pb::ReadingListSpecifics>
ReadingListEntry::AsReadingListSpecifics() const {
  std::unique_ptr<sync_pb::ReadingListSpecifics> pb_entry =
      std::make_unique<sync_pb::ReadingListSpecifics>();

  // URL is used as the key for the database and sync as there is only one entry
  // per URL.
  pb_entry->set_entry_id(URL().spec());
  pb_entry->set_title(Title());
  pb_entry->set_url(URL().spec());
  pb_entry->set_creation_time_us(CreationTime());
  pb_entry->set_first_read_time_us(FirstReadTime());
  pb_entry->set_update_time_us(UpdateTime());
  pb_entry->set_update_title_time_us(UpdateTitleTime());
  pb_entry->set_estimated_read_time_seconds(EstimatedReadTime().InSeconds());

  switch (state_) {
    case READ:
      pb_entry->set_status(sync_pb::ReadingListSpecifics::READ);
      break;
    case UNREAD:
      pb_entry->set_status(sync_pb::ReadingListSpecifics::UNREAD);
      break;
    case UNSEEN:
      pb_entry->set_status(sync_pb::ReadingListSpecifics::UNSEEN);
      break;
  }

  return pb_entry;
}
