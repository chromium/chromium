// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/printing/ppd_metadata_manager.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/queue.h"
#include "base/containers/span.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/printing/ppd_metadata_parser.h"
#include "chromeos/printing/ppd_provider.h"
#include "chromeos/printing/printer_config_cache.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos {

namespace {

// Defines the containing directory of all metadata in the serving root.
const char kMetadataParentDirectory[] = "metadata_v3";

// Defines the number of shards of sharded metadata.
constexpr int kNumShards = 20;

// Defines the magic maximal number for USB vendor IDs and product IDs
// (restricted to 16 bits).
constexpr int kSixteenBitsMaximum = 0xffff;

// Convenience struct containing parsed metadata of type T.
template <typename T>
struct ParsedMetadataWithTimestamp {
  base::Time time_of_parse;
  T value;
};

// Tracks the progress of a single call to
// PpdMetadataManager::FindAllEmmsAvailableInIndex().
class ForwardIndexSearchContext {
 public:
  ForwardIndexSearchContext(
      const std::vector<std::string>& emms,
      base::Time max_age,
      PpdMetadataManager::FindAllEmmsAvailableInIndexCallback cb)
      : emms_(emms), current_index_(), max_age_(max_age), cb_(std::move(cb)) {}
  ~ForwardIndexSearchContext() = default;

  ForwardIndexSearchContext(const ForwardIndexSearchContext&) = delete;
  ForwardIndexSearchContext& operator=(const ForwardIndexSearchContext&) =
      delete;

  ForwardIndexSearchContext(ForwardIndexSearchContext&&) = default;

  // The effective-make-and-model string currently being sought in the
  // forward index search tracked by this struct.
  base::StringPiece CurrentEmm() const {
    DCHECK_LT(current_index_, emms_.size());
    return emms_[current_index_];
  }

  // Returns whether the CurrentEmm() is the last one in |this|
  // that needs searching.
  bool CurrentEmmIsLast() const {
    DCHECK_LT(current_index_, emms_.size());
    return current_index_ + 1 == emms_.size();
  }

  void AdvanceToNextEmm() {
    DCHECK_LT(current_index_, emms_.size());
    current_index_++;
  }

  // Called when the PpdMetadataManager has searched all appropriate
  // forward index metadata for all |emms_|.
  void PostCallback() {
    DCHECK(CurrentEmmIsLast());
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(cb_), cb_arg_));
  }

  // Called when the PpdMetadataManager successfully maps the
  // CurrentEmm() to a ParsedIndexValues struct.
  void AddDataFromForwardIndexForCurrentEmm(const ParsedIndexValues& value) {
    cb_arg_.insert_or_assign(CurrentEmm(), value);
  }

  base::Time MaxAge() const { return max_age_; }

 private:
  // List of all effective-make-and-model strings that caller gave to
  // PpdMetadataManager::FindAllEmmsAvailableInIndex().
  std::vector<std::string> emms_;

  // Index into |emms| that marks the effective-make-and-model string
  // currently being searched.
  size_t current_index_;

  // Freshness requirement for forward indices that this search reads.
  base::Time max_age_;

  // Callback that caller gave to
  // PpdMetadataManager::FindAllEmmsAvailableInIndex().
  PpdMetadataManager::FindAllEmmsAvailableInIndexCallback cb_;

  // Accrues data to pass to |cb|.
  base::flat_map<std::string, ParsedIndexValues> cb_arg_;
};

// Enqueues calls to PpdMetadataManager::FindAllEmmsAvailableInIndex().
class ForwardIndexSearchQueue {
 public:
  ForwardIndexSearchQueue() = default;
  ~ForwardIndexSearchQueue() = default;

  ForwardIndexSearchQueue(const ForwardIndexSearchQueue&) = delete;
  ForwardIndexSearchQueue& operator=(const ForwardIndexSearchQueue&) = delete;

  void Enqueue(ForwardIndexSearchContext context) {
    contexts_.push(std::move(context));
  }

  bool IsIdle() const { return contexts_.empty(); }

  ForwardIndexSearchContext& CurrentContext() {
    DCHECK(!IsIdle());
    return contexts_.front();
  }

  // Progresses the frontmost search context, advancing it to its
  // next effective-make-and-model string to find in forward index
  // metadata.
  //
  // If the frontmost search context has no more
  // effective-make-and-model strings to search, then
  // 1. its callback is posted from here and
  // 2. it is popped off the |contexts| queue.
  void AdvanceToNextEmm() {
    DCHECK(!IsIdle());
    if (CurrentContext().CurrentEmmIsLast()) {
      CurrentContext().PostCallback();
      contexts_.pop();
    } else {
      CurrentContext().AdvanceToNextEmm();
    }
  }

 private:
  base::queue<ForwardIndexSearchContext> contexts_;
};

// Maps parsed metadata by name to parsed contents.
//
// Implementation note: the keys (metadata names) used here are
// basenames attached to their containing directory - e.g.
// *  "metadata_v3/index-00.json"
// *  "metadata_v3/locales.json"
// This is done to match up with the PrinterConfigCache class and
// with the folder layout of the Chrome OS Printing serving root.
template <typename T>
using CachedParsedMetadataMap =
    base::flat_map<std::string, ParsedMetadataWithTimestamp<T>>;

// Returns whether |map| has a value for |key| fresher than
// |expiration|.
template <typename T>
bool MapHasValueFresherThan(const CachedParsedMetadataMap<T>& metadata_map,
                            base::StringPiece key,
                            base::Time expiration) {
  if (!metadata_map.contains(key)) {
    return false;
  }
  const auto& value = metadata_map.at(key);
  return value.time_of_parse > expiration;
}

// Calculates the shard number of |key| inside sharded metadata.
int IndexShard(base::StringPiece key) {
  unsigned int hash = 5381;
  for (char c : key) {
    hash = hash * 33 + c;
  }
  return hash % kNumShards;
}

// Helper class used by PpdMetadataManagerImpl::SetMetadataLocale().
// Sifts through the list of locales advertised by the Chrome OS
// Printing serving root and selects the best match for a
// particular browser locale.
//
// This class must not outlive any data it is fed.
// This class is neither copyable nor movable.
class MetadataLocaleFinder {
 public:
  explicit MetadataLocaleFinder(const std::string& browser_locale)
      : browser_locale_(browser_locale),
        browser_locale_pieces_(base::SplitStringPiece(browser_locale,
                                                      "-",
                                                      base::KEEP_WHITESPACE,
                                                      base::SPLIT_WANT_ALL)),
        is_english_available_(false) {}
  ~MetadataLocaleFinder() = default;

  MetadataLocaleFinder(const MetadataLocaleFinder&) = delete;
  MetadataLocaleFinder& operator=(const MetadataLocaleFinder&) = delete;

  // Finds and returns the best-fit metadata locale from |locales|.
  // Returns the empty string if no best candidate was found.
  base::StringPiece BestCandidate(base::span<const std::string> locales) {
    AnalyzeCandidates(locales);

    if (!best_parent_locale_.empty()) {
      return best_parent_locale_;
    } else if (!best_distant_relative_locale_.empty()) {
      return best_distant_relative_locale_;
    } else if (is_english_available_) {
      return "en";
    }
    return base::StringPiece();
  }

 private:
  // Returns whether or not |locale| appears to be a parent of our
  // |browser_locale_|. For example, "en-GB" is a parent of "en-GB-foo."
  bool IsParentOfBrowserLocale(base::StringPiece locale) const {
    const std::string locale_with_trailing_hyphen = base::StrCat({locale, "-"});
    return base::StartsWith(browser_locale_, locale_with_trailing_hyphen);
  }

  // Updates our |best_distant_relative_locale_| to |locale| if we find
  // that it's a better match.
  //
  // The best distant relative locale is the one that
  // *  has the longest piecewise match with |browser_locale_| but
  // *  has the shortest piecewise length.
  // So given a |browser_locale_| "es," the better distant relative
  // locale between "es-GB" and "es-GB-foo" is "es-GB."
  void AnalyzeCandidateAsDistantRelative(base::StringPiece locale) {
    const std::vector<base::StringPiece> locale_pieces = base::SplitStringPiece(
        locale, "-", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);

    const size_t locale_piecewise_length = locale_pieces.size();
    const size_t iter_limit =
        std::min(browser_locale_pieces_.size(), locale_piecewise_length);
    size_t locale_piecewise_match_length = 0;
    for (; locale_piecewise_match_length < iter_limit;
         locale_piecewise_match_length++) {
      if (locale_pieces[locale_piecewise_match_length] !=
          browser_locale_pieces_[locale_piecewise_match_length]) {
        break;
      }
    }

    if (locale_piecewise_match_length == 0) {
      return;
    } else if (locale_piecewise_match_length >
                   best_distant_relative_locale_piecewise_match_length_ ||
               (locale_piecewise_match_length ==
                    best_distant_relative_locale_piecewise_match_length_ &&
                locale_piecewise_length <
                    best_distant_relative_locale_piecewise_length_)) {
      best_distant_relative_locale_ = std::string(locale);
      best_distant_relative_locale_piecewise_match_length_ =
          locale_piecewise_match_length;
      best_distant_relative_locale_piecewise_length_ = locale_piecewise_length;
    }
  }

  // Reads |locale| and updates our members as necessary.
  // For example, |locale| could reveal support for the "en" locale.
  void AnalyzeCandidate(base::StringPiece locale) {
    if (locale == "en") {
      is_english_available_ = true;
    }

    if (IsParentOfBrowserLocale(locale) &&
        locale.size() > best_parent_locale_.size()) {
      best_parent_locale_ = std::string(locale);
    } else if (best_parent_locale_.empty()) {
      // We need only track distant relative locales if we don't have a
      // |best_parent_locale_|, which is always a better choice.
      AnalyzeCandidateAsDistantRelative(locale);
    }
  }

  // Analyzes all candidate locales in |locales|, updating our
  // private members with best-fit locale(s).
  void AnalyzeCandidates(base::span<const std::string> locales) {
    for (base::StringPiece locale : locales) {
      // The serving root indicates direct support for our browser
      // locale; there's no need to analyze anything else, since this
      // is definitely the best match we're going to get.
      if (locale == browser_locale_) {
        best_parent_locale_ = std::string(browser_locale_);
        return;
      }
      AnalyzeCandidate(locale);
    }
  }

  const base::StringPiece browser_locale_;
  const std::vector<base::StringPiece> browser_locale_pieces_;

  // See IsParentOfBrowserLocale().
  std::string best_parent_locale_;

  // See AnalyzeCandidateAsDistantRelative().
  std::string best_distant_relative_locale_;
  size_t best_distant_relative_locale_piecewise_match_length_;
  size_t best_distant_relative_locale_piecewise_length_;

  // Denotes whether or not the Chrome OS Printing serving root serves
  // metadata for the "en" locale - our final fallback.
  bool is_english_available_;
};

// Represents the basename and containing directory of a piece of PPD
// metadata. Does not own any strings given to its setter methods and
// must not outlive them.
class PpdMetadataPathSpecifier {
 public:
  enum class Type {
    kLocales,
    kManufacturers,  // locale-sensitive
    kPrinters,       // locale-sensitive
    kForwardIndex,   // sharded
    kReverseIndex,   // locale-sensitive; sharded
    kUsbIndex,
    kUsbVendorIds,
  };

  explicit PpdMetadataPathSpecifier(Type type)
      : type_(type),
        printers_basename_(nullptr),
        metadata_locale_(nullptr),
        shard_(0),
        usb_vendor_id_(0) {}
  ~PpdMetadataPathSpecifier() = default;

  // PpdMetadataPathSpecifier is neither copyable nor movable.
  PpdMetadataPathSpecifier(const PpdMetadataPathSpecifier&) = delete;
  PpdMetadataPathSpecifier& operator=(const PpdMetadataPathSpecifier&) = delete;

  void SetPrintersBasename(const char* const basename) {
    DCHECK_EQ(type_, Type::kPrinters);
    printers_basename_ = basename;
  }

  void SetMetadataLocale(const char* const locale) {
    DCHECK(type_ == Type::kManufacturers || type_ == Type::kReverseIndex);
    metadata_locale_ = locale;
  }

  void SetUsbVendorId(const int vendor_id) {
    DCHECK_EQ(type_, Type::kUsbIndex);
    usb_vendor_id_ = vendor_id;
  }

  void SetShard(const int shard) {
    DCHECK(type_ == Type::kForwardIndex || type_ == Type::kReverseIndex);
    shard_ = shard;
  }

  std::string AsString() const {
    switch (type_) {
      case Type::kLocales:
        return base::StringPrintf("%s/locales.json", kMetadataParentDirectory);

      case Type::kManufacturers:
        DCHECK(metadata_locale_);
        DCHECK(!base::StringPiece(metadata_locale_).empty());
        return base::StringPrintf("%s/manufacturers-%s.json",
                                  kMetadataParentDirectory, metadata_locale_);

      case Type::kPrinters:
        DCHECK(printers_basename_);
        DCHECK(!base::StringPiece(printers_basename_).empty());
        return base::StringPrintf("%s/%s", kMetadataParentDirectory,
                                  printers_basename_);

      case Type::kForwardIndex:
        DCHECK(shard_ >= 0 && shard_ < kNumShards);
        return base::StringPrintf("%s/index-%02d.json",
                                  kMetadataParentDirectory, shard_);

      case Type::kReverseIndex:
        DCHECK(metadata_locale_);
        DCHECK(!base::StringPiece(metadata_locale_).empty());
        DCHECK(shard_ >= 0 && shard_ < kNumShards);
        return base::StringPrintf("%s/reverse_index-%s-%02d.json",
                                  kMetadataParentDirectory, metadata_locale_,
                                  shard_);

      case Type::kUsbIndex:
        DCHECK(usb_vendor_id_ >= 0 && usb_vendor_id_ <= kSixteenBitsMaximum);
        return base::StringPrintf("%s/usb-%04x.json", kMetadataParentDirectory,
                                  usb_vendor_id_);

      case Type::kUsbVendorIds:
        return base::StringPrintf("%s/usb_vendor_ids.json",
                                  kMetadataParentDirectory);
    }

    // This function cannot fail except by maintainer error.
    NOTREACHED();

    return std::string();
  }

  // Private const char* members are const char* for compatibility with
  // base::StringPrintf().
 private:
  Type type_;

  // Populated only when |type_| == kPrinters.
  // Contains the basename of the target printers metadata file.
  const char* printers_basename_;

  // Populated only when |type_| is locale-sensitive and != kPrinters.
  // Contains the metadata locale for which we intend to fetch metadata.
  const char* metadata_locale_;

  // Populated only when |type_| is sharded.
  int shard_;

  // Populated only when |type_| == kUsbIndex.
  int usb_vendor_id_;
};

// Note: generally, each Get*() method is segmented into three parts:
// 1. check if query can be answered immediately,
// 2. fetch appropriate metadata if it can't [defer to On*Fetched()],
//    and (time passes)
// 3. answer query with appropriate metadata [call On*Available()].
class PpdMetadataManagerImpl : public PpdMetadataManager {
 public:
  PpdMetadataManagerImpl(base::StringPiece browser_locale,
                         base::Clock* clock,
                         std::unique_ptr<PrinterConfigCache> config_cache)
      : browser_locale_(browser_locale),
        clock_(clock),
        config_cache_(std::move(config_cache)),
        weak_factory_(this) {}

  ~PpdMetadataManagerImpl() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  void GetLocale(GetLocaleCallback cb) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // Per header comment: if a best-fit metadata locale is already set,
    // we don't refresh it; we just immediately declare success.
    //
    // Side effect: classes composing |this| can call
    // SetLocaleForTesting() before composition and get this cop-out
    // for free.
    if (!metadata_locale_.empty()) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(cb), true));
      return;
    }

    PpdMetadataPathSpecifier path(PpdMetadataPathSpecifier::Type::kLocales);
    const std::string metadata_name = path.AsString();

    PrinterConfigCache::FetchCallback fetch_cb =
        base::BindOnce(&PpdMetadataManagerImpl::OnLocalesFetched,
                       weak_factory_.GetWeakPtr(), std::move(cb));

    // We call Fetch() with a default-constructed TimeDelta(): "give
    // me the freshest possible locales metadata."
    config_cache_->Fetch(metadata_name, base::TimeDelta(), std::move(fetch_cb));
  }

  void GetManufacturers(base::TimeDelta age,
                        PpdProvider::ResolveManufacturersCallback cb) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(!metadata_locale_.empty());

    PpdMetadataPathSpecifier path(
        PpdMetadataPathSpecifier::Type::kManufacturers);
    path.SetMetadataLocale(metadata_locale_.c_str());
    const std::string metadata_name = path.AsString();

    if (MapHasValueFresherThan(cached_manufacturers_, metadata_name,
                               clock_->Now() - age)) {
      OnManufacturersAvailable(metadata_name, std::move(cb));
      return;
    }

    PrinterConfigCache::FetchCallback fetch_cb =
        base::BindOnce(&PpdMetadataManagerImpl::OnManufacturersFetched,
                       weak_factory_.GetWeakPtr(), std::move(cb));
    config_cache_->Fetch(metadata_name, age, std::move(fetch_cb));
  }

  void GetPrinters(base::StringPiece manufacturer,
                   base::TimeDelta age,
                   GetPrintersCallback cb) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(!metadata_locale_.empty());

    const auto metadata_name = GetPrintersMetadataName(manufacturer);
    if (!metadata_name.has_value()) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(cb), false, ParsedPrinters{}));
      return;
    }

    if (MapHasValueFresherThan(cached_printers_, metadata_name.value(),
                               clock_->Now() - age)) {
      OnPrintersAvailable(metadata_name.value(), std::move(cb));
      return;
    }

    PrinterConfigCache::FetchCallback fetch_cb =
        base::BindOnce(&PpdMetadataManagerImpl::OnPrintersFetched,
                       weak_factory_.GetWeakPtr(), std::move(cb));
    config_cache_->Fetch(metadata_name.value(), age, std::move(fetch_cb));
  }

  void FindAllEmmsAvailableInIndex(
      const std::vector<std::string>& emms,
      base::TimeDelta age,
      FindAllEmmsAvailableInIndexCallback cb) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    ForwardIndexSearchContext context(emms, clock_->Now() - age, std::move(cb));
    bool queue_was_idle = forward_index_search_queue_.IsIdle();
    forward_index_search_queue_.Enqueue(std::move(context));

    // If we are the prime movers, then we need to set the forward
    // index search in motion.
    if (queue_was_idle) {
      ContinueSearchingForwardIndices();
    }

    // If we're not the prime movers, then a search is already ongoing
    // and we need not provide extra impetus.
  }

  void FindDeviceInUsbIndex(int vendor_id,
                            int product_id,
                            base::TimeDelta age,
                            FindDeviceInUsbIndexCallback cb) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // Fails the |cb| immediately if the |vendor_id| or |product_id| are
    // obviously out of range.
    if (vendor_id < 0 || vendor_id > kSixteenBitsMaximum || product_id < 0 ||
        product_id > kSixteenBitsMaximum) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(cb), std::string()));
      return;
    }

    PpdMetadataPathSpecifier path(PpdMetadataPathSpecifier::Type::kUsbIndex);
    path.SetUsbVendorId(vendor_id);
    const std::string metadata_name = path.AsString();

    if (MapHasValueFresherThan(cached_usb_indices_, metadata_name,
                               clock_->Now() - age)) {
      OnUsbIndexAvailable(metadata_name, product_id, std::move(cb));
      return;
    }

    auto callback = base::BindOnce(&PpdMetadataManagerImpl::OnUsbIndexFetched,
                                   weak_factory_.GetWeakPtr(), metadata_name,
                                   product_id, std::move(cb));
    config_cache_->Fetch(metadata_name, age, std::move(callback));
  }

  void GetUsbManufacturerName(int vendor_id,
                              base::TimeDelta age,
                              GetUsbManufacturerNameCallback cb) override {
    PpdMetadataPathSpecifier path(
        PpdMetadataPathSpecifier::Type::kUsbVendorIds);
    const std::string metadata_name = path.AsString();

    if (MapHasValueFresherThan(cached_usb_vendor_id_map_, metadata_name,
                               clock_->Now() - age)) {
      OnUsbVendorIdMapAvailable(metadata_name, vendor_id, std::move(cb));
      return;
    }

    auto fetch_cb =
        base::BindOnce(&PpdMetadataManagerImpl::OnUsbVendorIdMapFetched,
                       weak_factory_.GetWeakPtr(), vendor_id, std::move(cb));
    config_cache_->Fetch(metadata_name, age, std::move(fetch_cb));
  }

  void SplitMakeAndModel(base::StringPiece effective_make_and_model,
                         base::TimeDelta age,
                         PpdProvider::ReverseLookupCallback cb) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(!metadata_locale_.empty());

    PpdMetadataPathSpecifier path(
        PpdMetadataPathSpecifier::Type::kReverseIndex);
    path.SetMetadataLocale(metadata_locale_.c_str());
    path.SetShard(IndexShard(effective_make_and_model));
    const std::string metadata_name = path.AsString();

    if (MapHasValueFresherThan(cached_reverse_indices_, metadata_name,
                               clock_->Now() - age)) {
      OnReverseIndexAvailable(metadata_name, effective_make_and_model,
                              std::move(cb));
      return;
    }

    PrinterConfigCache::FetchCallback fetch_cb =
        base::BindOnce(&PpdMetadataManagerImpl::OnReverseIndexFetched,
                       weak_factory_.GetWeakPtr(),
                       std::string(effective_make_and_model), std::move(cb));
    config_cache_->Fetch(metadata_name, age, std::move(fetch_cb));
  }

  PrinterConfigCache* GetPrinterConfigCacheForTesting() const override {
    return config_cache_.get();
  }

  void SetLocaleForTesting(base::StringPiece locale) override {
    metadata_locale_ = std::string(locale);
  }

  // This method should read much the same as OnManufacturersFetched().
  bool SetManufacturersForTesting(
      base::StringPiece manufacturers_json) override {
    DCHECK(!metadata_locale_.empty());

    const auto parsed = ParseManufacturers(manufacturers_json);
    if (!parsed.has_value()) {
      return false;
    }

    // We need to name the manufacturers metadata manually to store it.
    PpdMetadataPathSpecifier path(
        PpdMetadataPathSpecifier::Type::kManufacturers);
    path.SetMetadataLocale(metadata_locale_.c_str());
    const std::string manufacturers_name = path.AsString();

    ParsedMetadataWithTimestamp<ParsedManufacturers> value = {clock_->Now(),
                                                              parsed.value()};
    cached_manufacturers_.insert_or_assign(manufacturers_name, value);
    return true;
  }

  base::StringPiece ExposeMetadataLocaleForTesting() const override {
    return metadata_locale_;
  }

 private:
  // Denotes the status of an ongoing forward index search - see
  // FindAllEmmsAvailableInIndex().
  enum class ForwardIndexSearchStatus {
    // We called |config_cache_|::Fetch(). We provided a bound
    // callback that will resume the forward index search for us when
    // the fetch completes.
    kWillResumeOnFetchCompletion,

    // We did not call |config_cache_|::Fetch(), so |this| still has
    // control of the progression of the forward index search.
    kCanContinue,
  };

  // Called by OnLocalesFetched().
  // Continues a prior call to GetLocale().
  //
  // Attempts to set |metadata_locale_| given the advertised
  // |locales_list|. Returns true if successful and false if not.
  bool SetMetadataLocale(const std::vector<std::string>& locales_list) {
    // This class helps track all the locales that _could_ be good fits
    // given our |browser_locale_| but which are not exact matches.
    MetadataLocaleFinder locale_finder(browser_locale_);

    metadata_locale_ = std::string(locale_finder.BestCandidate(locales_list));
    return !metadata_locale_.empty();
  }

  // Called back by |config_cache_|.Fetch().
  // Continues a prior call to GetLocale().
  //
  // On successful |result|, parses and sets the |metadata_locale_|.
  // Calls |cb| with the |result|.
  void OnLocalesFetched(GetLocaleCallback cb,
                        const PrinterConfigCache::FetchResult& result) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (!result.succeeded) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(cb), false));
      return;
    }
    const auto parsed = ParseLocales(result.contents);
    if (!parsed.has_value()) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(cb), false));
      return;
    }

    // SetMetadataLocale() _can_ fail, but that would be an
    // extraordinarily bad thing - i.e. that the Chrome OS Printing
    // serving root is itself in an invalid state.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(cb), SetMetadataLocale(parsed.value())));
  }

  // Called by one of
  // *  GetManufacturers() or
  // *  OnManufacturersFetched().
  // Continues a prior call to GetManufacturers().
  //
  // Invokes |cb| with success, providing it with a list of
  // manufacturers.
  void OnManufacturersAvailable(base::StringPiece metadata_name,
                                PpdProvider::ResolveManufacturersCallback cb) {
    const auto& parsed_manufacturers = cached_manufacturers_.at(metadata_name);
    std::vector<std::string> manufacturers_for_cb;
    for (const auto& iter : parsed_manufacturers.value) {
      manufacturers_for_cb.push_back(iter.first);
    }
    std::sort(manufacturers_for_cb.begin(), manufacturers_for_cb.end());
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(cb), PpdProvider::CallbackResultCode::SUCCESS,
                       manufacturers_for_cb));
  }

  // Called by |config_cache_|.Fetch().
  // Continues a prior call to GetManufacturers().
  //
  // Parses and updates our cached map of manufacturers if |result|
  // indicates a successful fetch. Calls |cb| accordingly.
  void OnManufacturersFetched(PpdProvider::ResolveManufacturersCallback cb,
                              const PrinterConfigCache::FetchResult& result) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (!result.succeeded) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(cb),
                         PpdProvider::CallbackResultCode::SERVER_ERROR,
                         std::vector<std::string>{}));
      return;
    }

    const auto parsed = ParseManufacturers(result.contents);
    if (!parsed.has_value()) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(cb),
                         PpdProvider::CallbackResultCode::INTERNAL_ERROR,
                         std::vector<std::string>{}));
      return;
    }

    ParsedMetadataWithTimestamp<ParsedManufacturers> value = {clock_->Now(),
                                                              parsed.value()};
    cached_manufacturers_.insert_or_assign(result.key, value);
    OnManufacturersAvailable(result.key, std::move(cb));
  }

  // Called by GetPrinters().
  // Returns the known name for the Printers metadata named by
  // |manufacturer|.
  absl::optional<std::string> GetPrintersMetadataName(
      base::StringPiece manufacturer) {
    PpdMetadataPathSpecifier manufacturers_path(
        PpdMetadataPathSpecifier::Type::kManufacturers);
    manufacturers_path.SetMetadataLocale(metadata_locale_.c_str());
    const std::string manufacturers_metadata_name =
        manufacturers_path.AsString();
    if (!cached_manufacturers_.contains(manufacturers_metadata_name)) {
      // This is likely a bug: we don't have the expected manufacturers
      // metadata.
      return absl::nullopt;
    }

    const ParsedMetadataWithTimestamp<ParsedManufacturers>& manufacturers =
        cached_manufacturers_.at(manufacturers_metadata_name);
    if (!manufacturers.value.contains(manufacturer)) {
      // This is likely a bug: we don't know about this manufacturer.
      return absl::nullopt;
    }

    PpdMetadataPathSpecifier printers_path(
        PpdMetadataPathSpecifier::Type::kPrinters);
    printers_path.SetPrintersBasename(
        manufacturers.value.at(manufacturer).c_str());
    return printers_path.AsString();
  }

  // Called by one of
  // *  GetPrinters() or
  // *  OnPrintersFetched().
  // Continues a prior call to GetPrinters().
  //
  // Invokes |cb| with success, providing it a map of printers.
  void OnPrintersAvailable(base::StringPiece metadata_name,
                           GetPrintersCallback cb) {
    const auto& parsed_printers = cached_printers_.at(metadata_name);
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(cb), true, parsed_printers.value));
  }

  // Called by |config_cache_|.Fetch().
  // Continues a prior call to GetPrinters().
  //
  // Parses and updates our cached map of printers if |result| indicates
  // a successful fetch. Calls |cb| accordingly.
  void OnPrintersFetched(GetPrintersCallback cb,
                         const PrinterConfigCache::FetchResult& result) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (!result.succeeded) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(cb), false, ParsedPrinters{}));
      return;
    }

    const auto parsed = ParsePrinters(result.contents);
    if (!parsed.has_value()) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(cb), false, ParsedPrinters{}));
      return;
    }

    ParsedMetadataWithTimestamp<ParsedPrinters> value = {clock_->Now(),
                                                         parsed.value()};
    cached_printers_.insert_or_assign(result.key, value);
    OnPrintersAvailable(result.key, std::move(cb));
  }

  // Called when one unit of sufficiently fresh forward index metadata
  // is available. Seeks out the current effective-make-and-model string
  // in said metadata.
  void FindEmmInForwardIndex(base::StringPiece metadata_name) {
    // Caller must have verified that this index is already present (and
    // sufficiently fresh) before entering this method.
    DCHECK(cached_forward_indices_.contains(metadata_name));

    ForwardIndexSearchContext& context =
        forward_index_search_queue_.CurrentContext();

    const ParsedIndex& index = cached_forward_indices_.at(metadata_name).value;
    const auto& iter = index.find(context.CurrentEmm());
    if (iter != index.end()) {
      context.AddDataFromForwardIndexForCurrentEmm(iter->second);
    }

    forward_index_search_queue_.AdvanceToNextEmm();
  }

  // Called by |config_cache_|.Fetch().
  // Continues a prior call to FindAllEmmsAvailableInForwardIndex().
  //
  // Parses and updates our cached map of forward indices if |result|
  // indicates a successful fetch. Continues the action that
  // necessitated fetching the present forward index.
  void OnForwardIndexFetched(const PrinterConfigCache::FetchResult& result) {
    if (!result.succeeded) {
      // We failed to fetch the forward index containing the current
      // effective-make-and-model string. There's nothing we can do but
      // carry on, e.g. by moving to deal with the next emm.
      forward_index_search_queue_.AdvanceToNextEmm();
      ContinueSearchingForwardIndices();
      return;
    }

    const auto parsed = ParseForwardIndex(result.contents);
    if (!parsed.has_value()) {
      // Same drill as fetch failure above.
      forward_index_search_queue_.AdvanceToNextEmm();
      ContinueSearchingForwardIndices();
      return;
    }
    ParsedMetadataWithTimestamp<ParsedIndex> value = {clock_->Now(),
                                                      parsed.value()};
    cached_forward_indices_.insert_or_assign(result.key, value);
    ContinueSearchingForwardIndices();
  }

  // Works on searching the forward index for the current
  // effective-make-and-model string in the frontmost entry in the
  // forward index search queue.
  //
  // One invocation of this method ultimately processes exactly one
  // effective-make-and-model string: either we find it in some forward
  // index metadata or we don't.
  ForwardIndexSearchStatus SearchForwardIndicesForOneEmm() {
    const ForwardIndexSearchContext& context =
        forward_index_search_queue_.CurrentContext();
    PpdMetadataPathSpecifier path(
        PpdMetadataPathSpecifier::Type::kForwardIndex);
    path.SetShard(IndexShard(context.CurrentEmm()));
    const std::string forward_index_name = path.AsString();

    if (MapHasValueFresherThan(cached_forward_indices_, forward_index_name,
                               context.MaxAge())) {
      // We have the appropriate forward index metadata and it's fresh
      // enough to make a determination: is the current
      // effective-make-and-model string  present in this metadata?
      FindEmmInForwardIndex(forward_index_name);
      return ForwardIndexSearchStatus::kCanContinue;
    }

    // We don't have the appropriate forward index metadata. We need to
    // get it before we can determine if the current
    // effective-make-and-model string is present in it.
    //
    // PrinterConfigCache::Fetch() accepts a TimeDelta expressing the
    // maximum permissible age of the cached response; to simulate the
    // original TimeDelta that caller gave to
    // FindAllEmmsAvailableInIndex(), we find the delta between Now()
    // and the absolute time ceiling recorded in the
    // ForwardIndexSearchContext.
    auto callback =
        base::BindOnce(&PpdMetadataManagerImpl::OnForwardIndexFetched,
                       weak_factory_.GetWeakPtr());
    config_cache_->Fetch(forward_index_name, clock_->Now() - context.MaxAge(),
                         std::move(callback));
    return ForwardIndexSearchStatus::kWillResumeOnFetchCompletion;
  }

  // Continues working on the forward index search queue.
  void ContinueSearchingForwardIndices() {
    while (!forward_index_search_queue_.IsIdle()) {
      ForwardIndexSearchStatus status = SearchForwardIndicesForOneEmm();

      // If we invoked |config_cache_|::Fetch(), then control has passed
      // out of this class for now. It will resume from
      // OnForwardIndexFetched().
      if (status == ForwardIndexSearchStatus::kWillResumeOnFetchCompletion) {
        break;
      }
    }
  }

  // Called by one of
  // *  FindDeviceInUsbIndex() or
  // *  OnUsbIndexFetched().
  // Searches the now-available USB index metadata with |metadata_name|
  // for a device with given |product_id|, calling |cb| appropriately.
  void OnUsbIndexAvailable(base::StringPiece metadata_name,
                           int product_id,
                           FindDeviceInUsbIndexCallback cb) {
    DCHECK(cached_usb_indices_.contains(metadata_name));

    const ParsedUsbIndex& usb_index =
        cached_usb_indices_.at(metadata_name).value;
    const auto& iter = usb_index.find(product_id);
    std::string effective_make_and_model;
    if (iter != usb_index.end()) {
      effective_make_and_model = iter->second;
    }

    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(cb), std::move(effective_make_and_model)));
  }

  // Called by |config_cache_|.Fetch().
  // Continues a prior call to FindDeviceInUsbIndex().
  //
  // Parses and updates our cached map of USB index metadata if |result|
  // indicates a successful fetch.
  void OnUsbIndexFetched(std::string metadata_name,
                         int product_id,
                         FindDeviceInUsbIndexCallback cb,
                         const PrinterConfigCache::FetchResult& fetch_result) {
    if (!fetch_result.succeeded) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(cb), std::string()));
      return;
    }

    absl::optional<ParsedUsbIndex> parsed =
        ParseUsbIndex(fetch_result.contents);
    if (!parsed.has_value()) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(cb), std::string()));
      return;
    }

    DCHECK(fetch_result.key == metadata_name);
    ParsedMetadataWithTimestamp<ParsedUsbIndex> value = {clock_->Now(),
                                                         parsed.value()};
    cached_usb_indices_.insert_or_assign(fetch_result.key, value);
    OnUsbIndexAvailable(fetch_result.key, product_id, std::move(cb));
  }

  // Called by one of
  // *  GetUsbManufacturerName() or
  // *  OnUsbVendorIdMapFetched().
  //
  // Searches the available USB vendor ID map (named by |metadata_name|)
  // for |vendor_id| and invokes |cb| accordingly.
  void OnUsbVendorIdMapAvailable(base::StringPiece metadata_name,
                                 int vendor_id,
                                 GetUsbManufacturerNameCallback cb) {
    DCHECK(cached_usb_vendor_id_map_.contains(metadata_name));
    ParsedUsbVendorIdMap usb_vendor_id_map =
        cached_usb_vendor_id_map_.at(metadata_name).value;

    std::string manufacturer_name;
    const auto& iter = usb_vendor_id_map.find(vendor_id);
    if (iter != usb_vendor_id_map.end()) {
      manufacturer_name = iter->second;
    }

    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(cb), manufacturer_name));
  }

  // Called by |config_cache_|->Fetch().
  // Continues a prior call to GetUsbManufacturerName.
  //
  // Parses and updates our cached map of USB vendor IDs if |result|
  // indicates a successful fetch.
  //
  // If we're haggling over bits, it is wasteful to have a map that
  // only ever has at most one key-value pair. We willfully accept this
  // inefficiency to maintain consistency with other metadata
  // operations.
  void OnUsbVendorIdMapFetched(
      int vendor_id,
      GetUsbManufacturerNameCallback cb,
      const PrinterConfigCache::FetchResult& fetch_result) {
    if (!fetch_result.succeeded) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(cb), std::string()));
      return;
    }

    const absl::optional<ParsedUsbVendorIdMap> parsed =
        ParseUsbVendorIdMap(fetch_result.contents);
    if (!parsed.has_value()) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(cb), std::string()));
      return;
    }

    ParsedMetadataWithTimestamp<ParsedUsbVendorIdMap> value = {clock_->Now(),
                                                               parsed.value()};
    cached_usb_vendor_id_map_.insert_or_assign(fetch_result.key, value);
    OnUsbVendorIdMapAvailable(fetch_result.key, vendor_id, std::move(cb));
  }

  // Called by one of
  // *  SplitMakeAndModel() or
  // *  OnReverseIndexFetched().
  // Continues a prior call to SplitMakeAndModel().
  //
  // Looks for |effective_make_and_model| in the reverse index named by
  // |metadata_name|, and tries to invoke |cb| with the split make and
  // model.
  void OnReverseIndexAvailable(base::StringPiece metadata_name,
                               base::StringPiece effective_make_and_model,
                               PpdProvider::ReverseLookupCallback cb) {
    const auto& parsed_reverse_index =
        cached_reverse_indices_.at(metadata_name);

    // We expect this reverse index shard to contain the decomposition
    // for |effective_make_and_model|.
    if (!parsed_reverse_index.value.contains(effective_make_and_model)) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(cb),
                         PpdProvider::CallbackResultCode::NOT_FOUND, "", ""));
      return;
    }

    const ReverseIndexLeaf& leaf =
        parsed_reverse_index.value.at(effective_make_and_model);

    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(cb), PpdProvider::CallbackResultCode::SUCCESS,
                       leaf.manufacturer, leaf.model));
  }

  // Called by |config_cache_|.Fetch().
  // Continues a prior call to SplitMakeAndModel().
  //
  // Parses and updates our cached map of reverse indices if |result|
  // indicates a successful fetch. Calls |cb| accordingly.
  void OnReverseIndexFetched(std::string effective_make_and_model,
                             PpdProvider::ReverseLookupCallback cb,
                             const PrinterConfigCache::FetchResult& result) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (!result.succeeded) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(cb),
                         PpdProvider::CallbackResultCode::SERVER_ERROR, "",
                         ""));
      return;
    }

    const auto parsed = ParseReverseIndex(result.contents);
    if (!parsed.has_value()) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(cb),
                         PpdProvider::CallbackResultCode::INTERNAL_ERROR, "",
                         ""));
      return;
    }

    ParsedMetadataWithTimestamp<ParsedReverseIndex> value = {clock_->Now(),
                                                             parsed.value()};
    cached_reverse_indices_.insert_or_assign(result.key, value);
    OnReverseIndexAvailable(result.key, effective_make_and_model,
                            std::move(cb));
  }

  const std::string browser_locale_;
  raw_ptr<const base::Clock> clock_;

  // The closest match to |browser_locale_| for which the serving root
  // claims to serve metadata.
  std::string metadata_locale_;

  std::unique_ptr<PrinterConfigCache> config_cache_;

  CachedParsedMetadataMap<ParsedManufacturers> cached_manufacturers_;
  CachedParsedMetadataMap<ParsedPrinters> cached_printers_;
  CachedParsedMetadataMap<ParsedIndex> cached_forward_indices_;
  CachedParsedMetadataMap<ParsedUsbIndex> cached_usb_indices_;
  CachedParsedMetadataMap<ParsedUsbVendorIdMap> cached_usb_vendor_id_map_;
  CachedParsedMetadataMap<ParsedReverseIndex> cached_reverse_indices_;

  // Processing queue for FindAllEmmsAvailableInIndex().
  ForwardIndexSearchQueue forward_index_search_queue_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Dispenses weak pointers to the |config_cache_|. This is necessary
  // because |this| could be deleted while the |config_cache_| is
  // processing something off-sequence.
  base::WeakPtrFactory<PpdMetadataManagerImpl> weak_factory_;
};

}  // namespace

// static
std::unique_ptr<PpdMetadataManager> PpdMetadataManager::Create(
    base::StringPiece browser_locale,
    base::Clock* clock,
    std::unique_ptr<PrinterConfigCache> config_cache) {
  return std::make_unique<PpdMetadataManagerImpl>(browser_locale, clock,
                                                  std::move(config_cache));
}

}  // namespace chromeos
