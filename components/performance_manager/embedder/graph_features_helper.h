// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_EMBEDDER_GRAPH_FEATURES_HELPER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_EMBEDDER_GRAPH_FEATURES_HELPER_H_

#include <cstdint>

namespace performance_manager {

class Graph;

// A helper for configuring and enabling Graph features. This object is
// constexpr so that it can be easily used with static storage without
// requiring an initializer.
class GraphFeaturesHelper {
 public:
  // Helper for housing the actual configuration data.
  union Flags {
    uint32_t flags;
    struct {
      // When adding new features here, the following also needs to happen:
      // (1) Add a corresponding EnableFeatureFoo() member function.
      // (2) Add the feature to EnableDefault() if necessary.
      // (3) Add the feature to the implementation of ConfigureGraph().
      bool execution_context_priority_decorator : 1;
      bool execution_context_registry : 1;
      bool frame_node_impl_describer : 1;
      bool frame_visibility_decorator : 1;
      bool freezing_vote_decorator : 1;
      bool page_live_state_decorator : 1;
      bool page_load_tracker_decorator : 1;
      bool page_node_impl_describer : 1;
      bool process_node_impl_describer : 1;
      bool site_data_recorder : 1;
      bool tab_properties_decorator : 1;
      bool v8_context_tracker : 1;
      bool worker_node_impl_describer : 1;
    };
  };

  constexpr GraphFeaturesHelper() = default;
  constexpr GraphFeaturesHelper(const GraphFeaturesHelper& other) = default;
  GraphFeaturesHelper& operator=(const GraphFeaturesHelper& other) = default;

  constexpr GraphFeaturesHelper& EnableExecutionContextPriorityDecorator() {
    EnableExecutionContextRegistry();
    flags_.execution_context_priority_decorator = true;
    return *this;
  }

  constexpr GraphFeaturesHelper& EnableExecutionContextRegistry() {
    flags_.execution_context_registry = true;
    return *this;
  }

  constexpr GraphFeaturesHelper& EnableFrameNodeImplDescriber() {
    flags_.frame_node_impl_describer = true;
    return *this;
  }

  constexpr GraphFeaturesHelper& EnableFrameVisibilityDecorator() {
    flags_.frame_visibility_decorator = true;
    return *this;
  }

  constexpr GraphFeaturesHelper& EnableFreezingVoteDecorator() {
    flags_.freezing_vote_decorator = true;
    return *this;
  }

  constexpr GraphFeaturesHelper& EnablePageLiveStateDecorator() {
    flags_.page_live_state_decorator = true;
    return *this;
  }

  constexpr GraphFeaturesHelper& EnablePageLoadTrackerDecorator() {
    flags_.page_load_tracker_decorator = true;
    return *this;
  }

  constexpr GraphFeaturesHelper& EnablePageNodeImplDescriber() {
    flags_.page_node_impl_describer = true;
    return *this;
  }

  constexpr GraphFeaturesHelper& EnableProcessNodeImplDescriber() {
    flags_.process_node_impl_describer = true;
    return *this;
  }

  // This is a nop on the Android platform, as the feature isn't available
  // there.
  constexpr GraphFeaturesHelper& EnableSiteDataRecorder() {
    flags_.site_data_recorder = true;
    return *this;
  }

  constexpr GraphFeaturesHelper& EnableTabPropertiesDecorator() {
    flags_.tab_properties_decorator = true;
    return *this;
  }

  constexpr GraphFeaturesHelper& EnableV8ContextTracker() {
    EnableExecutionContextRegistry();
    flags_.v8_context_tracker = true;
    return *this;
  }

  constexpr GraphFeaturesHelper& EnableWorkerNodeImplDescriber() {
    flags_.worker_node_impl_describer = true;
    return *this;
  }

  // Helper to enable the minimal set of features required for a content_shell
  // browser to work.
  constexpr GraphFeaturesHelper& EnableMinimal() {
    EnableExecutionContextRegistry();
    EnableV8ContextTracker();
    return *this;
  }

  // Helper to enable the default set of features. This is only intended for use
  // from production code.
  constexpr GraphFeaturesHelper& EnableDefault() {
    EnableExecutionContextRegistry();
    EnableFrameNodeImplDescriber();
    EnableFrameVisibilityDecorator();
    EnableFreezingVoteDecorator();
    EnablePageLiveStateDecorator();
    EnablePageLoadTrackerDecorator();
    EnablePageNodeImplDescriber();
    EnableProcessNodeImplDescriber();
    EnableSiteDataRecorder();
    EnableTabPropertiesDecorator();
    EnableV8ContextTracker();
    EnableWorkerNodeImplDescriber();
    return *this;
  }

  // Accessor for the current set of flags_.
  constexpr const Flags& flags() const { return flags_; }

  // Applies the configuration specified on this object to the provided
  // graph. This will unconditionally try to install all of the enabled
  // features, even if they have already been installed; it is generally
  // preferable to call this exactly once on a brand new |graph| instance that
  // has had no features installed. Otherwise, it is safe to call this to
  // install new features that the caller knows have not yet been installed.
  void ConfigureGraph(Graph* graph) const;

 private:
  Flags flags_ = {0};
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_EMBEDDER_GRAPH_FEATURES_HELPER_H_
