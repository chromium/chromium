// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ThreatDOMDetails iterates over a document's frames and gathers
// interesting URLs such as those of scripts and frames. When done, it sends
// them to the ThreatDetails that requested them.

#ifndef COMPONENTS_SAFE_BROWSING_RENDERER_THREAT_DOM_DETAILS_H_
#define COMPONENTS_SAFE_BROWSING_RENDERER_THREAT_DOM_DETAILS_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "components/safe_browsing/common/safe_browsing.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/service_manager/public/cpp/binder_registry.h"

namespace safe_browsing {

extern const char kTagAndAttributeParamName[];

// Represents the tag name of an HTML Element and its associated attributes.
// Used to determine which elements to collect. Populated from the param value
// of |kThreatDomDetailsTagAndAttributeFeature|.
class TagAndAttributesItem {
 public:
  TagAndAttributesItem();
  TagAndAttributesItem(const std::string& tag_name_param,
                       const std::vector<std::string>& attributes_param);
  TagAndAttributesItem(const TagAndAttributesItem& item);
  ~TagAndAttributesItem();

  std::string tag_name;
  std::vector<std::string> attributes;
};

// There is one ThreatDOMDetails per RenderFrame.
class ThreatDOMDetails : public content::RenderFrameObserver,
                         public mojom::ThreatReporter {
 public:
  // An upper limit on the number of nodes we collect. Not const for the test.
  static uint32_t kMaxNodes;

  // An upper limit on the number of attributes to collect per node. Not const
  // for the test.
  static uint32_t kMaxAttributes;

  // An upper limit on the length of an attribute string.
  static uint32_t kMaxAttributeStringLength;

  static ThreatDOMDetails* Create(content::RenderFrame* render_frame,
                                  service_manager::BinderRegistry* registry);
  ~ThreatDOMDetails() override;

  // Begins extracting resource urls for the page currently loaded in
  // this object's RenderFrame.
  // Exposed for testing.
  void ExtractResources(std::vector<mojom::ThreatDOMDetailsNodePtr>* resources);

  std::vector<TagAndAttributesItem> GetTagAndAttributesListForTest() {
    return tag_and_attributes_list_;
  }

 private:
  // Creates a ThreatDOMDetails for the specified RenderFrame.
  // The ThreatDOMDetails should be destroyed prior to destroying
  // the RenderFrame.
  ThreatDOMDetails(content::RenderFrame* render_frame,
                   service_manager::BinderRegistry* registry);

  void OnDestruct() override;

  // safe_browsing::mojom::ThreatReporter:
  void GetThreatDOMDetails(GetThreatDOMDetailsCallback callback) override;

  void OnThreatReporterReceiver(
      mojo::PendingReceiver<mojom::ThreatReporter> receiver);

  mojo::ReceiverSet<mojom::ThreatReporter> threat_reporter_receivers_;

  // A list of tag names and associates attributes, used to determine which
  // elements need to be collected.
  std::vector<TagAndAttributesItem> tag_and_attributes_list_;

  DISALLOW_COPY_AND_ASSIGN(ThreatDOMDetails);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_RENDERER_THREAT_DOM_DETAILS_H_
