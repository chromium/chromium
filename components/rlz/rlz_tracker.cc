// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This code glues the RLZ library DLL with Chrome. It allows Chrome to work
// with or without the DLL being present. If the DLL is not present the
// functions do nothing and just return false.

#include "components/rlz/rlz_tracker.h"

#include <algorithm>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/rlz/rlz_tracker_delegate.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader.mojom.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/syslog_logging.h"
#endif

namespace rlz {
namespace {

// Maximum and minimum delay for financial ping we would allow to be set through
// master preferences. Somewhat arbitrary, may need to be adjusted in future.
#if BUILDFLAG(IS_CHROMEOS_ASH)
const base::TimeDelta kMinInitDelay = base::Seconds(60);
const base::TimeDelta kMaxInitDelay = base::Hours(24);
#else
const base::TimeDelta kMinInitDelay = base::Seconds(20);
const base::TimeDelta kMaxInitDelay = base::Seconds(200);
#endif

void RecordProductEvents(bool first_run,
                         bool is_google_default_search,
                         bool is_google_homepage,
                         bool is_google_in_startpages,
                         bool already_ran,
                         bool omnibox_used,
                         bool homepage_used,
                         bool app_list_used) {
  TRACE_EVENT0("RLZ", "RecordProductEvents");
  // Record the installation of chrome. We call this all the time but the rlz
  // lib should ignore all but the first one.
  rlz_lib::RecordProductEvent(rlz_lib::CHROME,
                              RLZTracker::ChromeOmnibox(),
                              rlz_lib::INSTALL);
#if !BUILDFLAG(IS_IOS)
  rlz_lib::RecordProductEvent(rlz_lib::CHROME,
                              RLZTracker::ChromeHomePage(),
                              rlz_lib::INSTALL);
  rlz_lib::RecordProductEvent(rlz_lib::CHROME,
                              RLZTracker::ChromeAppList(),
                              rlz_lib::INSTALL);
#endif  // !BUILDFLAG(IS_IOS)

  if (!already_ran) {
    // Do the initial event recording if is the first run or if we have an
    // empty rlz which means we haven't got a chance to do it.
    char omnibox_rlz[rlz_lib::kMaxRlzLength + 1];
    if (!rlz_lib::GetAccessPointRlz(RLZTracker::ChromeOmnibox(), omnibox_rlz,
                                    rlz_lib::kMaxRlzLength)) {
      omnibox_rlz[0] = 0;
    }

    // Record if google is the initial search provider and/or home page.
    if ((first_run || omnibox_rlz[0] == 0) && is_google_default_search) {
      rlz_lib::RecordProductEvent(rlz_lib::CHROME,
                                  RLZTracker::ChromeOmnibox(),
                                  rlz_lib::SET_TO_GOOGLE);
    }

#if !BUILDFLAG(IS_IOS)
    char homepage_rlz[rlz_lib::kMaxRlzLength + 1];
    if (!rlz_lib::GetAccessPointRlz(RLZTracker::ChromeHomePage(), homepage_rlz,
                                    rlz_lib::kMaxRlzLength)) {
      homepage_rlz[0] = 0;
    }

    if ((first_run || homepage_rlz[0] == 0) &&
        (is_google_homepage || is_google_in_startpages)) {
      rlz_lib::RecordProductEvent(rlz_lib::CHROME,
                                  RLZTracker::ChromeHomePage(),
                                  rlz_lib::SET_TO_GOOGLE);
    }

    char app_list_rlz[rlz_lib::kMaxRlzLength + 1];
    if (!rlz_lib::GetAccessPointRlz(RLZTracker::ChromeAppList(), app_list_rlz,
                                    rlz_lib::kMaxRlzLength)) {
      app_list_rlz[0] = 0;
    }

    // Record if google is the initial search provider and/or home page.
    if ((first_run || app_list_rlz[0] == 0) && is_google_default_search) {
      rlz_lib::RecordProductEvent(rlz_lib::CHROME,
                                  RLZTracker::ChromeAppList(),
                                  rlz_lib::SET_TO_GOOGLE);
    }
#endif  // !BUILDFLAG(IS_IOS)
  }

  // Record first user interaction with the omnibox. We call this all the
  // time but the rlz lib should ignore all but the first one.
  if (omnibox_used) {
    rlz_lib::RecordProductEvent(rlz_lib::CHROME,
                                RLZTracker::ChromeOmnibox(),
                                rlz_lib::FIRST_SEARCH);
  }

#if !BUILDFLAG(IS_IOS)
  // Record first user interaction with the home page. We call this all the
  // time but the rlz lib should ignore all but the first one.
  if (homepage_used || is_google_in_startpages) {
    rlz_lib::RecordProductEvent(rlz_lib::CHROME,
                                RLZTracker::ChromeHomePage(),
                                rlz_lib::FIRST_SEARCH);
  }

  // Record first user interaction with the app list. We call this all the
  // time but the rlz lib should ignore all but the first one.
  if (app_list_used) {
    rlz_lib::RecordProductEvent(rlz_lib::CHROME,
                                RLZTracker::ChromeAppList(),
                                rlz_lib::FIRST_SEARCH);
  }
#endif  // !BUILDFLAG(IS_IOS)
}

bool SendFinancialPing(const std::string& brand,
                       const std::u16string& lang,
                       const std::u16string& referral) {
  rlz_lib::AccessPoint points[] = {
    RLZTracker::ChromeOmnibox(),
#if !BUILDFLAG(IS_IOS)
    RLZTracker::ChromeHomePage(),
    RLZTracker::ChromeAppList(),
#endif
    rlz_lib::NO_ACCESS_POINT
  };
  std::string lang_ascii(base::UTF16ToASCII(lang));
  std::string referral_ascii(base::UTF16ToASCII(referral));
  std::string product_signature;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  product_signature = "chromeos";
#else
  product_signature = "chrome";
#endif
  return rlz_lib::SendFinancialPing(
      rlz_lib::CHROME, points, product_signature.c_str(), brand.c_str(),
      referral_ascii.c_str(), lang_ascii.c_str(), false, true);
}

}  // namespace

RLZTracker* RLZTracker::tracker_ = nullptr;

// WrapperURLLoaderFactory subclasses mojom::URLLoaderFactory as non-mojo, cross
// thread class. It basically posts ::CreateLoaderAndStart calls over to the UI
// thread, to call them on the real mojo object.
class RLZTracker::WrapperURLLoaderFactory
    : public network::mojom::URLLoaderFactory {
 public:
  explicit WrapperURLLoaderFactory(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : url_loader_factory_(std::move(url_loader_factory)),
        main_thread_task_runner_(
            base::SequencedTaskRunner::GetCurrentDefault()) {}

  WrapperURLLoaderFactory(const WrapperURLLoaderFactory&) = delete;
  WrapperURLLoaderFactory& operator=(const WrapperURLLoaderFactory&) = delete;

  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override {
    if (main_thread_task_runner_->RunsTasksInCurrentSequence()) {
      url_loader_factory_->CreateLoaderAndStart(
          std::move(loader), request_id, options, request, std::move(client),
          traffic_annotation);
    } else {
      main_thread_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&WrapperURLLoaderFactory::CreateLoaderAndStart,
                         base::Unretained(this), std::move(loader), request_id,
                         options, request, std::move(client),
                         traffic_annotation));
    }
  }
  void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory)
      override {
    NOTIMPLEMENTED();
  }

 private:
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Runner for RLZ main thread tasks.
  scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner_;
};

// static
RLZTracker* RLZTracker::GetInstance() {
  static base::NoDestructor<RLZTracker> instance;
  return tracker_ ? tracker_ : instance.get();
}

RLZTracker::RLZTracker()
    : first_run_(false),
      send_ping_immediately_(false),
      is_google_default_search_(false),
      is_google_homepage_(false),
      is_google_in_startpages_(false),
      already_ran_(false),
      omnibox_used_(false),
      homepage_used_(false),
      app_list_used_(false),
      min_init_delay_(kMinInitDelay),
      background_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN, base::MayBlock(),
           base::TaskPriority::BEST_EFFORT})) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

RLZTracker::~RLZTracker() {
}

// static
void RLZTracker::SetRlzDelegate(std::unique_ptr<RLZTrackerDelegate> delegate) {
  RLZTracker* tracker = GetInstance();
  if (!tracker->delegate_) {
    // RLZTracker::SetRlzDelegate is called at Profile creation time which can
    // happens multiple time on ChromeOS, so do nothing if the delegate already
    // exists.
    tracker->SetDelegate(std::move(delegate));
  }
}

void RLZTracker::SetDelegate(std::unique_ptr<RLZTrackerDelegate> delegate) {
  DCHECK(delegate);
  DCHECK(!delegate_);
  delegate_ = std::move(delegate);
}

// static
void RLZTracker::ClearRlzDelegateForTesting() {
  GetInstance()->ClearDelegateForTesting();  // IN-TEST
}

void RLZTracker::ClearDelegateForTesting() {
  delegate_.reset();
}

// static
bool RLZTracker::InitRlzDelayed(bool first_run,
                                bool send_ping_immediately,
                                base::TimeDelta delay,
                                bool is_google_default_search,
                                bool is_google_homepage,
                                bool is_google_in_startpages) {
  return GetInstance()->Init(first_run, send_ping_immediately, delay,
                             is_google_default_search, is_google_homepage,
                             is_google_in_startpages);
}

bool RLZTracker::Init(bool first_run,
                      bool send_ping_immediately,
                      base::TimeDelta delay,
                      bool is_google_default_search,
                      bool is_google_homepage,
                      bool is_google_in_startpages) {
  DCHECK(delegate_) << "RLZTracker used before initialization";
  first_run_ = first_run;
  is_google_default_search_ = is_google_default_search;
  is_google_homepage_ = is_google_homepage;
  is_google_in_startpages_ = is_google_in_startpages;
  send_ping_immediately_ = send_ping_immediately;

  // Enable zero delays for testing.
  if (delegate_->ShouldEnableZeroDelayForTesting())
    EnableZeroDelayForTesting();

  delay = std::clamp(delay, min_init_delay_, kMaxInitDelay);

  if (delegate_->GetBrand(&brand_) && !delegate_->IsBrandOrganic(brand_)) {
    // Register for notifications from the omnibox so that we can record when
    // the user performs a first search.
    delegate_->SetOmniboxSearchCallback(
        base::BindOnce(&RLZTracker::RecordFirstSearch, base::Unretained(this),
                       ChromeOmnibox()));

#if !BUILDFLAG(IS_IOS)
    // Register for notifications from navigations, to see if the user has used
    // the home page.
    delegate_->SetHomepageSearchCallback(
        base::BindOnce(&RLZTracker::RecordFirstSearch, base::Unretained(this),
                       ChromeHomePage()));
#endif
  }
  delegate_->GetReactivationBrand(&reactivation_brand_);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // If the brand is organic, RLZ is essentially disabled.  Write a log to the
  // console for administrators and QA.
  if (delegate_->IsBrandOrganic(brand_) &&
      delegate_->IsBrandOrganic(reactivation_brand_)) {
    SYSLOG(INFO) << "RLZ is disabled";
  } else if (delegate_->ShouldUpdateExistingAccessPointRlz()) {
    background_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(
                       [](const std::string& brand) {
                         rlz_lib::UpdateExistingAccessPointRlz(brand);
                       },
                       brand_));
  }
#endif

  // Could be null; don't run if so.  RLZ will try again next restart.
  auto shared_url_loader_factory = delegate_->GetURLLoaderFactory();
  if (shared_url_loader_factory) {
    custom_url_loader_factory_ =
        std::make_unique<WrapperURLLoaderFactory>(shared_url_loader_factory);
    rlz_lib::SetURLLoaderFactory(custom_url_loader_factory_.get());
    ScheduleDelayedInit(delay);
  }

#if !BUILDFLAG(IS_IOS)
  // Prime the RLZ cache for the home page access point so that its available
  // for the startup page if needed (i.e., when the startup page is set to
  // the home page).
  GetAccessPointRlz(ChromeHomePage(), nullptr);
#endif  // !BUILDFLAG(IS_IOS)

  return true;
}

void RLZTracker::Cleanup() {
  {
    base::AutoLock lock(cache_lock_);
    rlz_cache_.clear();
  }
  if (delegate_)
    delegate_->Cleanup();
}

void RLZTracker::ScheduleDelayedInit(base::TimeDelta delay) {
  DCHECK(delegate_) << "RLZTracker used before initialization";
  // The RLZTracker is a singleton object that outlives any runnable tasks
  // that will be queued up.
  background_task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&RLZTracker::DelayedInit, base::Unretained(this)), delay);
}

void RLZTracker::DelayedInit() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(delegate_) << "RLZTracker used before initialization";
  bool schedule_ping = false;

  // For organic brandcodes do not use rlz at all. Empty brandcode usually
  // means a chromium install. This is ok.
  if (!delegate_->IsBrandOrganic(brand_)) {
    RecordProductEvents(first_run_, is_google_default_search_,
                        is_google_homepage_, is_google_in_startpages_,
                        already_ran_, omnibox_used_, homepage_used_,
                        app_list_used_);
    schedule_ping = true;
  }

  // If chrome has been reactivated, record the events for this brand
  // as well.
  if (!delegate_->IsBrandOrganic(reactivation_brand_)) {
    rlz_lib::SupplementaryBranding branding(reactivation_brand_.c_str());
    RecordProductEvents(first_run_, is_google_default_search_,
                        is_google_homepage_, is_google_in_startpages_,
                        already_ran_, omnibox_used_, homepage_used_,
                        app_list_used_);
    schedule_ping = true;
  }

  already_ran_ = true;

  if (schedule_ping)
    ScheduleFinancialPing();
}

void RLZTracker::ScheduleFinancialPing() {
  DCHECK(delegate_) << "RLZTracker used before initialization";
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&RLZTracker::PingNowImpl, base::Unretained(this)));
}

void RLZTracker::PingNowImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(delegate_) << "RLZTracker used before initialization";
  TRACE_EVENT0("RLZ", "RLZTracker::PingNowImpl");
  std::u16string lang;
  delegate_->GetLanguage(&lang);
  if (lang.empty())
    lang = u"en";
  std::u16string referral;
  delegate_->GetReferral(&referral);

  if (!delegate_->IsBrandOrganic(brand_) &&
      SendFinancialPing(brand_, lang, referral)) {
    delegate_->ClearReferral();

    {
      base::AutoLock lock(cache_lock_);
      rlz_cache_.clear();
    }

    // Prime the RLZ cache for the access points we are interested in.
    GetAccessPointRlz(RLZTracker::ChromeOmnibox(), nullptr);
#if !BUILDFLAG(IS_IOS)
    GetAccessPointRlz(RLZTracker::ChromeHomePage(), nullptr);
    GetAccessPointRlz(RLZTracker::ChromeAppList(), nullptr);
#endif  // !BUILDFLAG(IS_IOS)
  }

  if (!delegate_->IsBrandOrganic(reactivation_brand_)) {
    rlz_lib::SupplementaryBranding branding(reactivation_brand_.c_str());
    SendFinancialPing(reactivation_brand_, lang, referral);
  }
}

bool RLZTracker::SendFinancialPing(const std::string& brand,
                                   const std::u16string& lang,
                                   const std::u16string& referral) {
  return ::rlz::SendFinancialPing(brand, lang, referral);
}

// static
bool RLZTracker::RecordProductEvent(rlz_lib::Product product,
                                    rlz_lib::AccessPoint point,
                                    rlz_lib::Event event_id) {
  // This method is called during unit tests while the RLZTracker has not been
  // initialized, so check for the presence of a delegate and exit if there is
  // none registered.
  RLZTracker* tracker = GetInstance();
  return !tracker->delegate_ ? false : tracker->RecordProductEventImpl(
                                           product, point, event_id);
}

bool RLZTracker::RecordProductEventImpl(rlz_lib::Product product,
                                        rlz_lib::AccessPoint point,
                                        rlz_lib::Event event_id) {
  DCHECK(delegate_) << "RLZTracker used before initialization";
  // Make sure we don't access disk outside of the I/O thread.
  // In such case we repost the task on the right thread and return error.
  if (ScheduleRecordProductEvent(product, point, event_id))
    return true;

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool ret = rlz_lib::RecordProductEvent(product, point, event_id);

  // If chrome has been reactivated, record the event for this brand as well.
  if (!reactivation_brand_.empty()) {
    rlz_lib::SupplementaryBranding branding(reactivation_brand_.c_str());
    ret &= rlz_lib::RecordProductEvent(product, point, event_id);
  }

  return ret;
}

bool RLZTracker::ScheduleRecordProductEvent(rlz_lib::Product product,
                                            rlz_lib::AccessPoint point,
                                            rlz_lib::Event event_id) {
  DCHECK(delegate_) << "RLZTracker used before initialization";
  if (!delegate_->IsOnUIThread())
    return false;

  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(&RLZTracker::RecordProductEvent),
                     product, point, event_id));
  return true;
}

void RLZTracker::RecordFirstSearch(rlz_lib::AccessPoint point) {
  DCHECK(delegate_) << "RLZTracker used before initialization";
  // Make sure we don't access disk outside of the I/O thread.
  // In such case we repost the task on the right thread and return error.
  if (ScheduleRecordFirstSearch(point))
    return;

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if !BUILDFLAG(IS_IOS)
  if (point == ChromeHomePage()) {
    chrome_homepage_search_recorded_ = true;
  }
#endif  // !BUILDFLAG(IS_IOS)

  bool* record_used = GetAccessPointRecord(point);

  // Try to record event now, else set the flag to try later when we
  // attempt the ping.
  if (!RecordProductEvent(rlz_lib::CHROME, point, rlz_lib::FIRST_SEARCH)) {
    *record_used = true;
  } else if (send_ping_immediately_ && point == ChromeOmnibox()) {
    ScheduleDelayedInit(base::TimeDelta());
  }
}

bool RLZTracker::ScheduleRecordFirstSearch(rlz_lib::AccessPoint point) {
  DCHECK(delegate_) << "RLZTracker used before initialization";
  if (!delegate_->IsOnUIThread())
    return false;
  background_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&RLZTracker::RecordFirstSearch,
                                base::Unretained(this), point));
  return true;
}

bool* RLZTracker::GetAccessPointRecord(rlz_lib::AccessPoint point) {
  if (point == ChromeOmnibox())
    return &omnibox_used_;
#if !BUILDFLAG(IS_IOS)
  if (point == ChromeHomePage())
    return &homepage_used_;
  if (point == ChromeAppList())
    return &app_list_used_;
#endif  // !BUILDFLAG(IS_IOS)
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

// static
std::string RLZTracker::GetAccessPointHttpHeader(rlz_lib::AccessPoint point) {
  TRACE_EVENT0("RLZ", "RLZTracker::GetAccessPointHttpHeader");
  std::string extra_headers;
  std::u16string rlz_string;
  RLZTracker::GetAccessPointRlz(point, &rlz_string);
  if (!rlz_string.empty()) {
    return base::StringPrintf("X-Rlz-String: %s\r\n",
                              base::UTF16ToUTF8(rlz_string).c_str());
  }

  return extra_headers;
}

// GetAccessPointRlz() caches RLZ strings for all access points. If we had
// a successful ping, then we update the cached value.
// static
bool RLZTracker::GetAccessPointRlz(rlz_lib::AccessPoint point,
                                   std::u16string* rlz) {
  // This method is called during unit tests while the RLZTracker has not been
  // initialized, so check for the presence of a delegate and exit if there is
  // none registered.
  TRACE_EVENT0("RLZ", "RLZTracker::GetAccessPointRlz");
  RLZTracker* tracker = GetInstance();
  return !tracker->delegate_ ? false
                             : tracker->GetAccessPointRlzImpl(point, rlz);
}

// GetAccessPointRlz() caches RLZ strings for all access points. If we had
// a successful ping, then we update the cached value.
bool RLZTracker::GetAccessPointRlzImpl(rlz_lib::AccessPoint point,
                                       std::u16string* rlz) {
  DCHECK(delegate_) << "RLZTracker used before initialization";
  // If the RLZ string for the specified access point is already cached,
  // simply return its value.
  {
    base::AutoLock lock(cache_lock_);
    if (rlz_cache_.find(point) != rlz_cache_.end()) {
      if (rlz)
        *rlz = rlz_cache_[point];
      return true;
    }
  }

  // Make sure we don't access disk outside of the I/O thread.
  // In such case we repost the task on the right thread and return error.
  if (ScheduleGetAccessPointRlz(point))
    return false;

  char str_rlz[rlz_lib::kMaxRlzLength + 1];
  if (!rlz_lib::GetAccessPointRlz(point, str_rlz, rlz_lib::kMaxRlzLength))
    return false;

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::u16string rlz_local(base::ASCIIToUTF16(str_rlz));
  if (rlz)
    *rlz = rlz_local;

  base::AutoLock lock(cache_lock_);
  rlz_cache_[point] = rlz_local;
  return true;
}

bool RLZTracker::ScheduleGetAccessPointRlz(rlz_lib::AccessPoint point) {
  DCHECK(delegate_) << "RLZTracker used before initialization";
  if (!delegate_->IsOnUIThread())
    return false;

  std::u16string* not_used = nullptr;
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(&RLZTracker::GetAccessPointRlz), point,
                     not_used));
  return true;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// static
void RLZTracker::ClearRlzState() {
  RLZTracker* tracker = GetInstance();
  if (tracker->delegate_)
    tracker->ClearRlzStateImpl();
}

void RLZTracker::ClearRlzStateImpl() {
  DCHECK(delegate_) << "RLZTracker used before initialization";
  if (ScheduleClearRlzState())
    return;

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  rlz_lib::ClearAllProductEvents(rlz_lib::CHROME);
}

bool RLZTracker::ScheduleClearRlzState() {
  DCHECK(delegate_) << "RLZTracker used before initialization";
  if (!delegate_->IsOnUIThread())
    return false;

  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&RLZTracker::ClearRlzStateImpl, base::Unretained(this)));
  return true;
}
#endif

// static
void RLZTracker::CleanupRlz() {
  GetInstance()->Cleanup();
  rlz_lib::SetURLLoaderFactory(nullptr);
}

// static
void RLZTracker::EnableZeroDelayForTesting() {
  GetInstance()->min_init_delay_ = base::TimeDelta();
}

#if !BUILDFLAG(IS_IOS)
// static
void RLZTracker::RecordAppListSearch() {
  // This method is called during unit tests while the RLZTracker has not been
  // initialized, so check for the presence of a delegate and exit if there is
  // none registered.
  RLZTracker* tracker = GetInstance();
  if (tracker->delegate_)
    tracker->RecordFirstSearch(RLZTracker::ChromeAppList());
}

// static
bool RLZTracker::ShouldRecordChromeHomePageSearch() {
  RLZTracker* tracker = GetInstance();
  return tracker->delegate_ && tracker->delegate_->GetBrand(&tracker->brand_) &&
         !tracker->delegate_->IsBrandOrganic(tracker->brand_) &&
         !tracker->chrome_homepage_search_recorded_;
}

// static
void RLZTracker::RecordChromeHomePageSearch() {
  RLZTracker* tracker = GetInstance();
  if (tracker->delegate_ && tracker->ShouldRecordChromeHomePageSearch()) {
    tracker->delegate_->RunHomepageSearchCallback();
  }
}

// static
void RLZTracker::SetRlzChromeHomePageSearchRecordedForTesting(bool recorded) {
  GetInstance()->SetChromeHomePageSearchRecordedForTesting(  // IN-TEST
      recorded);
}

void RLZTracker::SetChromeHomePageSearchRecordedForTesting(bool recorded) {
  chrome_homepage_search_recorded_ = recorded;
}
#endif  // !BUILDFLAG(IS_IOS)

}  // namespace rlz
