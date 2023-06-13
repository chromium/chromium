// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_EMBEDDER_GRAPH_FEATURES_H_
#define COMPONENTS_PERFORMANCE_MANAGER_EMBEDDER_GRAPH_FEATURES_H_

#include <cstdint>

namespace performance_manager {

class Graph;

// A helper for configuring and enabling Graph features. This object is
// constexpr so that it can be easily used with static storage without
// requiring an initializer.
class GraphFeatures {
 public:
  // Returns a configuration with no graph features. Mostly intended for
  // unittests, where tests pick and choose exactly which decorators they want.
  static constexpr GraphFeatures WithNone() { return GraphFeatures(); }

  // Returns a configuration with the minimal set of graph features required
  // for content_shell to work.
  static constexpr GraphFeatures WithMinimal() {
    return GraphFeatures().EnableMinimal();
  }

  // Returns a configuration with the default set of graph features shipped
  // with a full-featured Chromium browser.
  static constexpr GraphFeatures WithDefault() {
    return GraphFeatures().EnableDefault();
  }

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
      bool frame_visibility_decorator : 1;
      bool freezing_vote_decorator : 1;
      bool metrics_collector : 1;
      bool node_impl_describers : 1;
      bool page_load_tracker_decorator : 1;
      bool process_hosted_content_types_aggregator : 1;
      bool site_data_recorder : 1;
      bool tab_properties_decorator : 1;
      bool v8_context_tracker : 1;
    };
  };

  constexpr GraphFeatures() = default;
  constexpr GraphFeatures(const GraphFeatures& other) = default;
  GraphFeatures& operator=(const GraphFeatures& other) = default;

  constexpr GraphFeatures& EnableExecutionContextPriorityDecorator() {
    EnableExecutionContextRegistry();
    EnableFrameVisibilityDecorator();
    flags_.execution_context_priority_decorator = true;
    return *this;
  }

  constexpr GraphFeatures& EnableExecutionContextRegistry() {
    flags_.execution_context_registry = true;
    return *this;
  }

  constexpr GraphFeatures& EnableNodeImplDescribers() {
    flags_.node_impl_describers = true;
    return *this;
  }

  constexpr GraphFeatures& EnableFrameVisibilityDecorator() {
    flags_.frame_visibility_decorator = true;
    return *this;
  }

  constexpr GraphFeatures& EnableMetricsCollector() {
    flags_.metrics_collector = true;
    return *this;
  }

  constexpr GraphFeatures& EnableFreezingVoteDecorator() {
    flags_.freezing_vote_decorator = true;
    return *this;
  }

  constexpr GraphFeatures& EnablePageLoadTrackerDecorator() {
    flags_.page_load_tracker_decorator = true;
    return *this;
  }

  constexpr GraphFeatures& EnableProcessHostedContentTypesAggregator() {
    flags_.process_hosted_content_types_aggregator = true;
    return *this;
  }

  // This is a nop on the Android platform, as the feature isn't available
  // there.
  constexpr GraphFeatures& EnableSiteDataRecorder() {
    flags_.site_data_recorder = true;
    return *this;
  }

  constexpr GraphFeatures& EnableTabPropertiesDecorator() {
    flags_.tab_properties_decorator = true;
    return *this;
  }

  constexpr GraphFeatures& EnableV8ContextTracker() {
    EnableExecutionContextRegistry();
    flags_.v8_context_tracker = true;
    return *this;
  }

  // Helper to enable the minimal set of features required for a content_shell
  // browser to work.
  constexpr GraphFeatures& EnableMinimal() {
    EnableExecutionContextRegistry();
    EnableV8ContextTracker();
    return *this;
  }

  // Helper to enable the default set of features. This is only intended for use
  // from production code.
  constexpr GraphFeatures& EnableDefault() {
    EnableExecutionContextRegistry();
    EnableFrameVisibilityDecorator();
    EnableFreezingVoteDecorator();
    EnableMetricsCollector();
    EnableNodeImplDescribers();
    EnablePageLoadTrackerDecorator();
    EnableProcessHostedContentTypesAggregator();
    EnableSiteDataRecorder();
    EnableTabPropertiesDecorator();
    EnableV8ContextTracker();
    EnableExecutionContextPriorityDecorator();
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

#endif  // COMPONENTS_PERFORMANCE_MANAGER_EMBEDDER_GRAPH_FEATURES_H_
