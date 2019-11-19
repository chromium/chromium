// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implementation of the ThreatDetails class.

#include "components/safe_browsing/browser/threat_details.h"

#include <stddef.h>
#include <stdint.h>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "components/history/core/browser/history_service.h"
#include "components/safe_browsing/base_ui_manager.h"
#include "components/safe_browsing/browser/referrer_chain_provider.h"
#include "components/safe_browsing/browser/threat_details_cache.h"
#include "components/safe_browsing/browser/threat_details_history.h"
#include "components/safe_browsing/db/hit_report.h"
#include "components/safe_browsing/features.h"
#include "components/safe_browsing/web_ui/safe_browsing_ui.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/service_manager/public/cpp/interface_provider.h"

using content::BrowserThread;
using content::NavigationEntry;
using content::RenderFrameHost;
using content::WebContents;

// Keep in sync with KMaxNodes in components/safe_browsing/renderer/
// threat_dom_details.cc
static const uint32_t kMaxDomNodes = 500;

namespace safe_browsing {

// static
ThreatDetailsFactory* ThreatDetails::factory_ = nullptr;

namespace {

// An element ID indicating that an HTML Element has no parent.
const int kElementIdNoParent = -1;

// The number of user gestures to trace back for the referrer chain.
const int kThreatDetailsUserGestureLimit = 2;

typedef std::unordered_set<std::string> StringSet;
// A set of HTTPS headers that are allowed to be collected. Contains both
// request and response headers. All entries in this list should be lower-case
// to support case-insensitive comparison.
struct WhitelistedHttpsHeadersTraits
    : base::internal::DestructorAtExitLazyInstanceTraits<StringSet> {
  static StringSet* New(void* instance) {
    StringSet* headers =
        base::internal::DestructorAtExitLazyInstanceTraits<StringSet>::New(
            instance);
    headers->insert({"google-creative-id", "google-lineitem-id", "referer",
                     "content-type", "content-length", "date", "server",
                     "cache-control", "pragma", "expires"});
    return headers;
  }
};
base::LazyInstance<StringSet, WhitelistedHttpsHeadersTraits>
    g_https_headers_whitelist = LAZY_INSTANCE_INITIALIZER;

// Helper function that converts SBThreatType to
// ClientSafeBrowsingReportRequest::ReportType.
ClientSafeBrowsingReportRequest::ReportType GetReportTypeFromSBThreatType(
    SBThreatType threat_type) {
  switch (threat_type) {
    case SB_THREAT_TYPE_URL_PHISHING:
      return ClientSafeBrowsingReportRequest::URL_PHISHING;
    case SB_THREAT_TYPE_URL_MALWARE:
      return ClientSafeBrowsingReportRequest::URL_MALWARE;
    case SB_THREAT_TYPE_URL_UNWANTED:
      return ClientSafeBrowsingReportRequest::URL_UNWANTED;
    case SB_THREAT_TYPE_URL_CLIENT_SIDE_PHISHING:
      return ClientSafeBrowsingReportRequest::URL_CLIENT_SIDE_PHISHING;
    case SB_THREAT_TYPE_URL_CLIENT_SIDE_MALWARE:
      return ClientSafeBrowsingReportRequest::URL_CLIENT_SIDE_MALWARE;
    case SB_THREAT_TYPE_BLOCKED_AD_POPUP:
      return ClientSafeBrowsingReportRequest::BLOCKED_AD_POPUP;
    case SB_THREAT_TYPE_AD_SAMPLE:
      return ClientSafeBrowsingReportRequest::AD_SAMPLE;
    case SB_THREAT_TYPE_BLOCKED_AD_REDIRECT:
      return ClientSafeBrowsingReportRequest::BLOCKED_AD_REDIRECT;
    case SB_THREAT_TYPE_SAVED_PASSWORD_REUSE:
    case SB_THREAT_TYPE_SIGNED_IN_SYNC_PASSWORD_REUSE:
    case SB_THREAT_TYPE_SIGNED_IN_NON_SYNC_PASSWORD_REUSE:
    case SB_THREAT_TYPE_ENTERPRISE_PASSWORD_REUSE:
      return ClientSafeBrowsingReportRequest::URL_PASSWORD_PROTECTION_PHISHING;
    case SB_THREAT_TYPE_SUSPICIOUS_SITE:
      return ClientSafeBrowsingReportRequest::URL_SUSPICIOUS;
    case SB_THREAT_TYPE_BILLING:
      return ClientSafeBrowsingReportRequest::BILLING;
    case SB_THREAT_TYPE_APK_DOWNLOAD:
      return ClientSafeBrowsingReportRequest::APK_DOWNLOAD;
    case SB_THREAT_TYPE_UNUSED:
    case SB_THREAT_TYPE_SAFE:
    case SB_THREAT_TYPE_URL_BINARY_MALWARE:
    case SB_THREAT_TYPE_EXTENSION:
    case SB_THREAT_TYPE_BLACKLISTED_RESOURCE:
    case SB_THREAT_TYPE_API_ABUSE:
    case SB_THREAT_TYPE_SUBRESOURCE_FILTER:
    case SB_THREAT_TYPE_CSD_WHITELIST:
    case SB_THREAT_TYPE_HIGH_CONFIDENCE_ALLOWLIST:
    case DEPRECATED_SB_THREAT_TYPE_URL_PASSWORD_PROTECTION_PHISHING:
      // Gated by SafeBrowsingBlockingPage::ShouldReportThreatDetails.
      NOTREACHED() << "We should not send report for threat type: "
                   << threat_type;
      return ClientSafeBrowsingReportRequest::UNKNOWN;
  }
}

// Clears the specified HTTPS resource of any sensitive data, only retaining
// data that is whitelisted for collection.
void ClearHttpsResource(ClientSafeBrowsingReportRequest::Resource* resource) {
  // Make a copy of the original resource to retain all data.
  ClientSafeBrowsingReportRequest::Resource orig_resource(*resource);

  // Clear the request headers and copy over any whitelisted ones.
  resource->clear_request();
  for (int i = 0; i < orig_resource.request().headers_size(); ++i) {
    ClientSafeBrowsingReportRequest::HTTPHeader* orig_header =
        orig_resource.mutable_request()->mutable_headers(i);
    if (g_https_headers_whitelist.Get().count(
            base::ToLowerASCII(orig_header->name())) > 0) {
      resource->mutable_request()->add_headers()->Swap(orig_header);
    }
  }
  // Also copy some other request fields.
  resource->mutable_request()->mutable_bodydigest()->swap(
      *orig_resource.mutable_request()->mutable_bodydigest());
  resource->mutable_request()->set_bodylength(
      orig_resource.request().bodylength());

  // ...repeat for response headers.
  resource->clear_response();
  for (int i = 0; i < orig_resource.response().headers_size(); ++i) {
    ClientSafeBrowsingReportRequest::HTTPHeader* orig_header =
        orig_resource.mutable_response()->mutable_headers(i);
    if (g_https_headers_whitelist.Get().count(
            base::ToLowerASCII(orig_header->name())) > 0) {
      resource->mutable_response()->add_headers()->Swap(orig_header);
    }
  }
  // Also copy some other response fields.
  resource->mutable_response()->mutable_bodydigest()->swap(
      *orig_resource.mutable_response()->mutable_bodydigest());
  resource->mutable_response()->set_bodylength(
      orig_resource.response().bodylength());
  resource->mutable_response()->mutable_remote_ip()->swap(
      *orig_resource.mutable_response()->mutable_remote_ip());
}

std::string GetElementKey(const int frame_tree_node_id,
                          const int element_node_id) {
  return base::StringPrintf("%d-%d", frame_tree_node_id, element_node_id);
}

using CSBRR = safe_browsing::ClientSafeBrowsingReportRequest;
CSBRR::SafeBrowsingUrlApiType GetUrlApiTypeForThreatSource(
    safe_browsing::ThreatSource source) {
  switch (source) {
    case safe_browsing::ThreatSource::DATA_SAVER:
      return CSBRR::FLYWHEEL;
    case safe_browsing::ThreatSource::LOCAL_PVER3:
      return CSBRR::PVER3_NATIVE;
    case safe_browsing::ThreatSource::LOCAL_PVER4:
      return CSBRR::PVER4_NATIVE;
    case safe_browsing::ThreatSource::REMOTE:
      return CSBRR::ANDROID_SAFETYNET;
    case safe_browsing::ThreatSource::UNKNOWN:
    case safe_browsing::ThreatSource::CLIENT_SIDE_DETECTION:
    case safe_browsing::ThreatSource::PASSWORD_PROTECTION_SERVICE:
      break;
  }
  return CSBRR::SAFE_BROWSING_URL_API_TYPE_UNSPECIFIED;
}

void TrimElements(const std::set<int> target_ids,
                  ElementMap* elements,
                  ResourceMap* resources) {
  if (target_ids.empty()) {
    elements->clear();
    resources->clear();
    return;
  }

  // First, scan over the elements and create a list ordered by element ID as
  // well as a reverse mapping from element ID to its parent ID.
  std::vector<HTMLElement*> elements_by_id(elements->size());

  // The parent vector is initialized with |kElementIdNoParent| so we can
  // identify elements that have no parent.
  std::vector<int> element_id_to_parent_id(elements->size(),
                                           kElementIdNoParent);
  for (const auto& element_pair : *elements) {
    HTMLElement* element = element_pair.second.get();
    elements_by_id[element->id()] = element;

    for (int child_id : element->child_ids()) {
      element_id_to_parent_id[child_id] = element->id();
    }
  }

  // Create a similar map for resources, ordered by resource ID.
  std::vector<std::string> resource_id_to_url(resources->size());
  for (const auto& resource_pair : *resources) {
    const std::string& url = resource_pair.first;
    ClientSafeBrowsingReportRequest::Resource* resource =
        resource_pair.second.get();
    resource_id_to_url[resource->id()] = url;
  }

  // Take a second pass and determine which element IDs to keep. We want to keep
  // the immediate parent, the siblings, and the children of the target ids.
  // By keeping the parent of the target and all of its children, this covers
  // the target's siblings as well.
  std::vector<int> element_ids_to_keep;
  // Resource IDs are also tracked so that we remember which resources are
  // attached to elements that we are keeping. This avoids deleting resources
  // that are shared between kept elements and trimmed elements.
  std::vector<int> kept_resource_ids;
  for (int target_id : target_ids) {
    const int parent_id = element_id_to_parent_id[target_id];
    if (parent_id == kElementIdNoParent) {
      // If one of the target elements has no parent then we skip trimming the
      // report further. Since we collect all siblings of this element, it will
      // effectively span the whole report, so no trimming necessary.
      return;
    }

    // Otherwise, insert the parent ID into the list of ids to keep. This will
    // capture the parent and siblings of the target element, as well as each of
    // their children.
    if (!base::Contains(element_ids_to_keep, parent_id)) {
      element_ids_to_keep.push_back(parent_id);

      // Check if this element has a resource. If so, remember to also keep the
      // resource.
      const HTMLElement& elem = *elements_by_id[parent_id];
      if (elem.has_resource_id()) {
        kept_resource_ids.push_back(elem.resource_id());
      }
    }
  }

  // Walk through |element_ids_to_keep| and append the children of each of
  // element to |element_ids_to_keep|. This is effectively a breadth-first
  // traversal of the tree. The list will stop growing when we reach the leaf
  // nodes that have no more children.
  for (size_t index = 0; index < element_ids_to_keep.size(); ++index) {
    int cur_element_id = element_ids_to_keep[index];
    const HTMLElement& element = *(elements_by_id[cur_element_id]);
    if (element.has_resource_id()) {
      kept_resource_ids.push_back(element.resource_id());
    }
    for (int child_id : element.child_ids()) {
      element_ids_to_keep.push_back(child_id);

      // Check if each child element has a resource. If so, remember to also
      // keep the resource.
      const HTMLElement& child_element = *elements_by_id[child_id];
      if (child_element.has_resource_id()) {
        kept_resource_ids.push_back(child_element.resource_id());
      }
    }
  }
  // Sort the list for easier lookup below.
  std::sort(element_ids_to_keep.begin(), element_ids_to_keep.end());

  // Now we know which elements we want to keep, scan through |elements| and
  // erase anything that we aren't keeping.
  for (auto element_iter = elements->begin();
       element_iter != elements->end();) {
    const HTMLElement& element = *element_iter->second;

    // Delete any elements that we do not want to keep.
    if (!base::Contains(element_ids_to_keep, element.id())) {
      // If this element has a resource then maybe delete the resouce too. Some
      // resources may be shared between kept and trimmed elements, and those
      // ones should not be deleted.
      if (element.has_resource_id() &&
          !base::Contains(kept_resource_ids, element.resource_id())) {
        const std::string& resource_url =
            resource_id_to_url[element.resource_id()];
        resources->erase(resource_url);
      }
      element_iter = elements->erase(element_iter);
    } else {
      ++element_iter;
    }
  }
}

}  // namespace

// The default ThreatDetailsFactory.  Global, made a singleton so we
// don't leak it.
class ThreatDetailsFactoryImpl : public ThreatDetailsFactory {
 public:
  std::unique_ptr<ThreatDetails> CreateThreatDetails(
      BaseUIManager* ui_manager,
      WebContents* web_contents,
      const security_interstitials::UnsafeResource& unsafe_resource,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      history::HistoryService* history_service,
      ReferrerChainProvider* referrer_chain_provider,
      bool trim_to_ad_tags,
      ThreatDetailsDoneCallback done_callback) override {
    // We can't use make_unique due to the protected constructor. We can't
    // directly use std::unique_ptr<ThreatDetails>(new ThreatDetails(...))
    // due to presubmit errors. So we use base::WrapUnique:
    auto threat_details = base::WrapUnique(new ThreatDetails(
        ui_manager, web_contents, unsafe_resource, url_loader_factory,
        history_service, referrer_chain_provider, trim_to_ad_tags,
        done_callback));
    threat_details->StartCollection();
    return threat_details;
  }

 private:
  friend struct base::LazyInstanceTraitsBase<ThreatDetailsFactoryImpl>;

  ThreatDetailsFactoryImpl() {}

  DISALLOW_COPY_AND_ASSIGN(ThreatDetailsFactoryImpl);
};

static base::LazyInstance<ThreatDetailsFactoryImpl>::DestructorAtExit
    g_threat_details_factory_impl = LAZY_INSTANCE_INITIALIZER;

// Create a ThreatDetails for the given tab.
/* static */
std::unique_ptr<ThreatDetails> ThreatDetails::NewThreatDetails(
    BaseUIManager* ui_manager,
    WebContents* web_contents,
    const UnsafeResource& resource,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    history::HistoryService* history_service,
    ReferrerChainProvider* referrer_chain_provider,
    bool trim_to_ad_tags,
    ThreatDetailsDoneCallback done_callback) {
  // Set up the factory if this has not been done already (tests do that
  // before this method is called).
  if (!factory_)
    factory_ = g_threat_details_factory_impl.Pointer();
  return factory_->CreateThreatDetails(
      ui_manager, web_contents, resource, url_loader_factory, history_service,
      referrer_chain_provider, trim_to_ad_tags, done_callback);
}

// Create a ThreatDetails for the given tab. Runs in the UI thread.
ThreatDetails::ThreatDetails(
    BaseUIManager* ui_manager,
    content::WebContents* web_contents,
    const UnsafeResource& resource,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    history::HistoryService* history_service,
    ReferrerChainProvider* referrer_chain_provider,
    bool trim_to_ad_tags,
    ThreatDetailsDoneCallback done_callback)
    : content::WebContentsObserver(web_contents),
      url_loader_factory_(url_loader_factory),
      ui_manager_(ui_manager),
      resource_(resource),
      referrer_chain_provider_(referrer_chain_provider),
      cache_result_(false),
      did_proceed_(false),
      num_visits_(0),
      trim_to_ad_tags_(trim_to_ad_tags),
      cache_collector_(new ThreatDetailsCacheCollector),
      done_callback_(done_callback),
      all_done_expected_(false),
      is_all_done_(false) {
  redirects_collector_ = new ThreatDetailsRedirectsCollector(
      history_service ? history_service->AsWeakPtr()
                      : base::WeakPtr<history::HistoryService>());
}

// TODO(lpz): Consider making this constructor delegate to the parameterized one
// above.
ThreatDetails::ThreatDetails()
    : cache_result_(false),
      did_proceed_(false),
      num_visits_(0),
      trim_to_ad_tags_(false),
      all_done_expected_(false),
      is_all_done_(false) {}

ThreatDetails::~ThreatDetails() {
  DCHECK_EQ(all_done_expected_, is_all_done_);
}

bool ThreatDetails::IsReportableUrl(const GURL& url) const {
  // TODO(panayiotis): also skip internal urls.
  return url.SchemeIs("http") || url.SchemeIs("https");
}

// Looks for a Resource for the given url in resources_.  If found, it
// updates |resource|. Otherwise, it creates a new message, adds it to
// resources_ and updates |resource| to point to it.
//
ClientSafeBrowsingReportRequest::Resource* ThreatDetails::FindOrCreateResource(
    const GURL& url) {
  auto& resource = resources_[url.spec()];
  if (!resource) {
    // Create the resource for |url|.
    int id = resources_.size() - 1;
    std::unique_ptr<ClientSafeBrowsingReportRequest::Resource> new_resource(
        new ClientSafeBrowsingReportRequest::Resource());
    new_resource->set_url(url.spec());
    new_resource->set_id(id);
    resource = std::move(new_resource);
  }
  return resource.get();
}

HTMLElement* ThreatDetails::FindOrCreateElement(
    const std::string& element_key) {
  auto& element = elements_[element_key];
  if (!element) {
    // Create an entry for this element.
    int element_dom_id = elements_.size() - 1;
    std::unique_ptr<HTMLElement> new_element(new HTMLElement());
    new_element->set_id(element_dom_id);
    element = std::move(new_element);
  }
  return element.get();
}

ClientSafeBrowsingReportRequest::Resource* ThreatDetails::AddUrl(
    const GURL& url,
    const GURL& parent,
    const std::string& tagname,
    const std::vector<GURL>* children) {
  if (!url.is_valid() || !IsReportableUrl(url))
    return nullptr;

  // Find (or create) the resource for the url.
  ClientSafeBrowsingReportRequest::Resource* url_resource =
      FindOrCreateResource(url);
  if (!tagname.empty())
    url_resource->set_tag_name(tagname);
  if (!parent.is_empty() && IsReportableUrl(parent)) {
    // Add the resource for the parent.
    ClientSafeBrowsingReportRequest::Resource* parent_resource =
        FindOrCreateResource(parent);
    // Update the parent-child relation
    url_resource->set_parent_id(parent_resource->id());
  }
  if (children) {
    for (auto it = children->begin(); it != children->end(); ++it) {
      // TODO(lpz): Should this first check if the child URL is reportable
      // before creating the resource?
      ClientSafeBrowsingReportRequest::Resource* child_resource =
          FindOrCreateResource(*it);
      bool duplicate_child = false;
      for (auto child_id : url_resource->child_ids()) {
        if (child_id == child_resource->id()) {
          duplicate_child = true;
          break;
        }
      }
      if (!duplicate_child)
        url_resource->add_child_ids(child_resource->id());
    }
  }

  return url_resource;
}

void ThreatDetails::AddDomElement(
    const int frame_tree_node_id,
    const int element_node_id,
    const std::string& tagname,
    const int parent_element_node_id,
    const std::vector<mojom::AttributeNameValuePtr> attributes,
    const std::string& inner_html,
    const ClientSafeBrowsingReportRequest::Resource* resource) {
  // Create the element. It should not exist already since this function should
  // only be called once for each element.
  const std::string element_key =
      GetElementKey(frame_tree_node_id, element_node_id);
  HTMLElement* cur_element = FindOrCreateElement(element_key);

  // Set some basic metadata about the element.
  const std::string tag_name_upper = base::ToUpperASCII(tagname);
  if (!tag_name_upper.empty()) {
    cur_element->set_tag(tag_name_upper);
  }
  for (const mojom::AttributeNameValuePtr& attribute : attributes) {
    HTMLElement::Attribute* attribute_pb = cur_element->add_attribute();
    attribute_pb->set_name(std::move(attribute->name));
    attribute_pb->set_value(std::move(attribute->value));

    // Remember which the IDs of elements that represent ads so we can trim the
    // report down to just those parts later.
    if (trim_to_ad_tags_ && attribute_pb->name() == "data-google-query-id") {
      trimmed_dom_element_ids_.insert(cur_element->id());
    }
  }

  if (!inner_html.empty()) {
    cur_element->set_inner_html(inner_html);
  }

  if (resource) {
    cur_element->set_resource_id(resource->id());
  }

  // Next we try to lookup the parent of the current element and add ourselves
  // as a child of it.
  HTMLElement* parent_element = nullptr;
  if (parent_element_node_id == 0) {
    // No parent indicates that this element is at the top of the current frame.
    // Remember that this is a top-level element of the frame with the
    // current |frame_tree_node_id|. If this element is inside an iframe, a
    // second pass will insert this element as a child of its parent iframe.
    frame_tree_id_to_children_map_[frame_tree_node_id].insert(
        cur_element->id());
  } else {
    // We have a parent ID, so this element is just a child of something inside
    // of our current frame. We can easily lookup our parent.
    const std::string& parent_key =
        GetElementKey(frame_tree_node_id, parent_element_node_id);
    if (base::Contains(elements_, parent_key)) {
      parent_element = elements_[parent_key].get();
    }
  }

  // If a parent element was found, add ourselves as a child, ensuring not to
  // duplicate child IDs.
  if (parent_element) {
    bool duplicate_child = false;
    for (const int child_id : parent_element->child_ids()) {
      if (child_id == cur_element->id()) {
        duplicate_child = true;
        break;
      }
    }
    if (!duplicate_child) {
      parent_element->add_child_ids(cur_element->id());
    }
  }
}

void ThreatDetails::StartCollection() {
  DVLOG(1) << "Starting to compute threat details.";
  report_.reset(new ClientSafeBrowsingReportRequest());

  if (IsReportableUrl(resource_.url)) {
    report_->set_url(resource_.url.spec());
    report_->set_type(GetReportTypeFromSBThreatType(resource_.threat_type));
  }

  GURL referrer_url;
  GURL page_url;

  // With committed interstitials, the information is pre-filled into the
  // UnsafeResource, since the navigation entry we have at this point is for the
  // navigation to the interstitial, and the entry with the page details get
  // destroyed when leaving the interstitial.
  if (!resource_.navigation_url.is_empty()) {
    DCHECK(
        base::FeatureList::IsEnabled(safe_browsing::kCommittedSBInterstitials));
    page_url = resource_.navigation_url;
    referrer_url = resource_.referrer_url;
  } else {
    NavigationEntry* nav_entry = resource_.GetNavigationEntryForResource();
    if (nav_entry) {
      page_url = nav_entry->GetURL();
      referrer_url = nav_entry->GetReferrer().url;
    }
  }

  if (IsReportableUrl(page_url))
    report_->set_page_url(page_url.spec());

  if (IsReportableUrl(referrer_url))
    report_->set_referrer_url(referrer_url.spec());

  // Add the nodes, starting from the page url.
  AddUrl(page_url, GURL(), std::string(), nullptr);

  // Add the resource_url and its original url, if non-empty and different.
  if (!resource_.original_url.is_empty() &&
      resource_.url != resource_.original_url) {
    // Add original_url, as the parent of resource_url.
    AddUrl(resource_.original_url, GURL(), std::string(), nullptr);
    AddUrl(resource_.url, resource_.original_url, std::string(), nullptr);
  } else {
    AddUrl(resource_.url, GURL(), std::string(), nullptr);
  }

  // Add the redirect urls, if non-empty. The redirect urls do not include the
  // original url, but include the unsafe url which is the last one of the
  // redirect urls chain
  GURL parent_url;
  // Set the original url as the parent of the first redirect url if it's not
  // empty.
  if (!resource_.original_url.is_empty())
    parent_url = resource_.original_url;

  // Set the previous redirect url as the parent of the next one
  for (size_t i = 0; i < resource_.redirect_urls.size(); ++i) {
    AddUrl(resource_.redirect_urls[i], parent_url, std::string(), nullptr);
    parent_url = resource_.redirect_urls[i];
  }

  // Add the referrer url.
  if (!referrer_url.is_empty())
    AddUrl(referrer_url, GURL(), std::string(), nullptr);

  if (!resource_.IsMainPageLoadBlocked()) {
    // Get URLs of frames, scripts etc from the DOM.
    // OnReceivedThreatDOMDetails will be called when the renderer replies.
    // TODO(mattm): In theory, if the user proceeds through the warning DOM
    // detail collection could be started once the page loads.
    web_contents()->ForEachFrame(base::BindRepeating(
        &ThreatDetails::RequestThreatDOMDetails, GetWeakPtr()));
  }
}

void ThreatDetails::RequestThreatDOMDetails(content::RenderFrameHost* frame) {
  content::BackForwardCache::DisableForRenderFrameHost(
      frame, "safe_browsing::ThreatDetails");
  mojo::Remote<safe_browsing::mojom::ThreatReporter> threat_reporter;
  frame->GetRemoteInterfaces()->GetInterface(
      threat_reporter.BindNewPipeAndPassReceiver());
  safe_browsing::mojom::ThreatReporter* raw_threat_report =
      threat_reporter.get();
  pending_render_frame_hosts_.push_back(frame);
  raw_threat_report->GetThreatDOMDetails(
      base::BindOnce(&ThreatDetails::OnReceivedThreatDOMDetails, GetWeakPtr(),
                     std::move(threat_reporter), frame));
}

// When the renderer is done, this is called.
void ThreatDetails::OnReceivedThreatDOMDetails(
    mojo::Remote<mojom::ThreatReporter> threat_reporter,
    content::RenderFrameHost* sender,
    std::vector<mojom::ThreatDOMDetailsNodePtr> params) {
  // If the RenderFrameHost was closed between sending the IPC and this callback
  // running, |sender| will be invalid.
  const auto sender_it = std::find(pending_render_frame_hosts_.begin(),
                                   pending_render_frame_hosts_.end(), sender);
  if (sender_it == pending_render_frame_hosts_.end()) {
    return;
  }

  pending_render_frame_hosts_.erase(sender_it);

  // Lookup the FrameTreeNode ID of any child frames in the list of DOM nodes.
  const int sender_process_id = sender->GetProcess()->GetID();
  const int sender_frame_tree_node_id = sender->GetFrameTreeNodeId();
  KeyToFrameTreeIdMap child_frame_tree_map;
  for (const mojom::ThreatDOMDetailsNodePtr& node : params) {
    if (node->child_frame_routing_id == 0)
      continue;

    const std::string cur_element_key =
        GetElementKey(sender_frame_tree_node_id, node->node_id);
    int child_frame_tree_node_id =
        content::RenderFrameHost::GetFrameTreeNodeIdForRoutingId(
            sender_process_id, node->child_frame_routing_id);
    if (child_frame_tree_node_id !=
        content::RenderFrameHost::kNoFrameTreeNodeId) {
      child_frame_tree_map[cur_element_key] = child_frame_tree_node_id;
    }
  }

  AddDOMDetails(sender_frame_tree_node_id, std::move(params),
                child_frame_tree_map);
}

void ThreatDetails::AddDOMDetails(
    const int frame_tree_node_id,
    std::vector<mojom::ThreatDOMDetailsNodePtr> params,
    const KeyToFrameTreeIdMap& child_frame_tree_map) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DVLOG(1) << "Nodes from the DOM: " << params.size();

  // If we have already started getting redirects from history service,
  // don't modify state, otherwise will invalidate the iterators.
  if (redirects_collector_->HasStarted())
    return;

  // If we have already started collecting data from the HTTP cache, don't
  // modify our state.
  if (cache_collector_->HasStarted())
    return;

  // Exit early if there are no nodes to process.
  if (params.empty())
    return;

  // Copy FrameTreeNode IDs for the child frame into the combined mapping.
  iframe_key_to_frame_tree_id_map_.insert(child_frame_tree_map.begin(),
                                          child_frame_tree_map.end());

  // Add the urls from the DOM to |resources_|. The renderer could be sending
  // bogus messages, so limit the number of nodes we accept.
  // Also update |elements_| with the DOM structure.
  for (size_t i = 0; i < params.size() && i < kMaxDomNodes; ++i) {
    mojom::ThreatDOMDetailsNode& node = *params[i];
    DVLOG(1) << node.url << ", " << node.tag_name << ", " << node.parent;
    ClientSafeBrowsingReportRequest::Resource* resource = nullptr;
    if (!node.url.is_empty()) {
      resource = AddUrl(node.url, node.parent, node.tag_name, &(node.children));
    }
    // Check for a tag_name to avoid adding the summary node to the DOM.
    if (!node.tag_name.empty()) {
      AddDomElement(frame_tree_node_id, node.node_id, node.tag_name,
                    node.parent_node_id, std::move(node.attributes),
                    node.inner_html, resource);
    }
  }
}

// Called from the SB Service on the IO thread, after the user has
// closed the tab, or clicked proceed or goback.  Since the user needs
// to take an action, we expect this to be called after
// OnReceivedThreatDOMDetails in most cases. If not, we don't include
// the DOM data in our report.
void ThreatDetails::FinishCollection(bool did_proceed, int num_visit) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  all_done_expected_ = true;

  // Do a second pass over the elements and update iframe elements to have
  // references to their children. Children may have been received from a
  // different renderer than the iframe element.
  for (auto& element_pair : elements_) {
    const std::string& element_key = element_pair.first;
    HTMLElement* element = element_pair.second.get();
    if (base::Contains(iframe_key_to_frame_tree_id_map_, element_key)) {
      int frame_tree_id_of_iframe_renderer =
          iframe_key_to_frame_tree_id_map_[element_key];
      const std::unordered_set<int>& child_ids =
          frame_tree_id_to_children_map_[frame_tree_id_of_iframe_renderer];
      for (const int child_id : child_ids) {
        element->add_child_ids(child_id);
      }
    }
  }

  did_proceed_ = did_proceed;
  num_visits_ = num_visit;
  std::vector<GURL> urls;
  for (ResourceMap::const_iterator it = resources_.begin();
       it != resources_.end(); ++it) {
    urls.push_back(GURL(it->first));
  }
  redirects_collector_->StartHistoryCollection(
      urls,
      base::Bind(&ThreatDetails::OnRedirectionCollectionReady, GetWeakPtr()));
}

void ThreatDetails::OnRedirectionCollectionReady() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  const std::vector<RedirectChain>& redirects =
      redirects_collector_->GetCollectedUrls();

  for (size_t i = 0; i < redirects.size(); ++i)
    AddRedirectUrlList(redirects[i]);

  // Call the cache collector
  cache_collector_->StartCacheCollection(
      url_loader_factory_, &resources_, &cache_result_,
      base::Bind(&ThreatDetails::OnCacheCollectionReady, GetWeakPtr()));
}

void ThreatDetails::AddRedirectUrlList(const std::vector<GURL>& urls) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (size_t i = 0; i < urls.size() - 1; ++i) {
    AddUrl(urls[i], urls[i + 1], std::string(), nullptr);
  }
}

void ThreatDetails::OnCacheCollectionReady() {
  DVLOG(1) << "OnCacheCollectionReady.";

  // All URLs have been collected, trim the report if necessary.
  if (trim_to_ad_tags_) {
    TrimElements(trimmed_dom_element_ids_, &elements_, &resources_);
    // If trimming the report removed all the elements then don't bother
    // sending it.
    if (elements_.empty()) {
      AllDone();
      return;
    }
  }
  // Add all the urls in our |resources_| maps to the |report_| protocol buffer.
  for (auto& resource_pair : resources_) {
    ClientSafeBrowsingReportRequest::Resource* pb_resource =
        report_->add_resources();
    pb_resource->Swap(resource_pair.second.get());
    const GURL url(pb_resource->url());
    if (url.SchemeIs("https")) {
      // Sanitize the HTTPS resource by clearing out private data (like cookie
      // headers).
      DVLOG(1) << "Clearing out HTTPS resource: " << pb_resource->url();
      ClearHttpsResource(pb_resource);
      // Keep id, parent_id, child_ids, and tag_name.
    }
  }
  for (auto& element_pair : elements_) {
    report_->add_dom()->Swap(element_pair.second.get());
  }

  report_->set_did_proceed(did_proceed_);
  // Only sets repeat_visit if num_visits_ >= 0.
  if (num_visits_ >= 0) {
    report_->set_repeat_visit(num_visits_ > 0);
  }
  report_->set_complete(cache_result_);

  report_->mutable_client_properties()->set_url_api_type(
      GetUrlApiTypeForThreatSource(resource_.threat_source));

  // Fill the referrer chain if applicable.
  MaybeFillReferrerChain();

  // Send the report, using the SafeBrowsingService.
  std::string serialized;
  if (!report_->SerializeToString(&serialized)) {
    DLOG(ERROR) << "Unable to serialize the threat report.";
    AllDone();
    return;
  }

  base::PostTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&WebUIInfoSingleton::AddToCSBRRsSent,
                     base::Unretained(WebUIInfoSingleton::GetInstance()),
                     std::move(report_)));

  ui_manager_->SendSerializedThreatDetails(serialized);

  AllDone();
}

void ThreatDetails::MaybeFillReferrerChain() {
  if (!referrer_chain_provider_)
    return;

  if (!report_ ||
      (report_->type() != ClientSafeBrowsingReportRequest::URL_SUSPICIOUS &&
       report_->type() != ClientSafeBrowsingReportRequest::APK_DOWNLOAD)) {
    return;
  }

  referrer_chain_provider_->IdentifyReferrerChainByWebContents(
      web_contents(), kThreatDetailsUserGestureLimit,
      report_->mutable_referrer_chain());
}

void ThreatDetails::AllDone() {
  is_all_done_ = true;
  base::PostTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(done_callback_, base::Unretained(web_contents())));
}

void ThreatDetails::FrameDeleted(RenderFrameHost* render_frame_host) {
  auto render_frame_host_it =
      std::find(pending_render_frame_hosts_.begin(),
                pending_render_frame_hosts_.end(), render_frame_host);
  if (render_frame_host_it != pending_render_frame_hosts_.end()) {
    pending_render_frame_hosts_.erase(render_frame_host_it);
  }
}

void ThreatDetails::RenderFrameHostChanged(RenderFrameHost* old_host,
                                           RenderFrameHost* new_host) {
  FrameDeleted(old_host);
}

base::WeakPtr<ThreatDetails> ThreatDetails::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace safe_browsing
