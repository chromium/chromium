// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/process_node_impl_describer.h"

#include "base/i18n/time_formatting.h"
#include "base/numerics/safe_conversions.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/task_traits.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/performance_manager/public/graph/node_data_describer_registry.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "content/public/common/child_process_host.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/win_util.h"
#endif

namespace performance_manager {

namespace {

const char kDescriberName[] = "ProcessNodeImpl";

std::string ContentTypeToString(ProcessNode::ContentType content_type) {
  switch (content_type) {
    case ProcessNode::ContentType::kExtension:
      return "Extension";
    case ProcessNode::ContentType::kMainFrame:
      return "Main frame";
    case ProcessNode::ContentType::kSubframe:
      return "Subframe";
    case ProcessNode::ContentType::kNavigatedFrame:
      return "Navigated Frame";
    case ProcessNode::ContentType::kAd:
      return "Ad";
    case ProcessNode::ContentType::kWorker:
      return "Worker";
  }
}

std::string HostedProcessTypesToString(
    ProcessNode::ContentTypes hosted_content_types) {
  std::vector<std::string> content_types_vector;
  content_types_vector.reserve(hosted_content_types.Size());
  for (ProcessNode::ContentType content_type : hosted_content_types)
    content_types_vector.push_back(ContentTypeToString(content_type));

  std::string str = base::JoinString(content_types_vector, ", ");
  if (str.empty())
    str = "none";

  return str;
}

base::Value GetProcessValueDict(const base::Process& process) {
  base::Value ret(base::Value::Type::DICTIONARY);

  // On Windows, handle is a void *. On Fuchsia it's an int. On other platforms
  // it is equal to the pid, so don't bother to record it.
#if BUILDFLAG(IS_WIN)
  ret.SetIntKey("handle", base::win::HandleToUint32(process.Handle()));
#elif BUILDFLAG(IS_FUCHSIA)
  ret.SetIntKey("handle", process.Handle());
#endif

  // Most processes are not current, so only show the outliers.
  if (process.is_current()) {
    ret.SetBoolKey("is_current", true);
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (process.GetPidInNamespace() != base::kNullProcessId) {
    ret.SetIntKey("pid_in_namespace", process.GetPidInNamespace());
  }
#endif

#if BUILDFLAG(IS_WIN)
  // Creation time is always available on Windows, even for dead processes.
  // On other platforms it is available only for valid processes (see below).
  ret.SetStringKey("creation_time", base::TimeFormatTimeOfDayWithMilliseconds(
                                        process.CreationTime()));
#endif

  if (process.IsValid()) {
    // These properties can only be accessed for valid processes.
    ret.SetIntKey("os_priority", process.GetPriority());
#if !BUILDFLAG(IS_APPLE)
    ret.SetBoolKey("is_backgrounded", process.IsProcessBackgrounded());
#endif
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_WIN)
    ret.SetStringKey("creation_time", base::TimeFormatTimeOfDayWithMilliseconds(
                                          process.CreationTime()));
#endif
#if BUILDFLAG(IS_WIN)
    // Most processes are running, so only show the outliers.
    if (!process.IsRunning()) {
      ret.SetBoolKey("is_running", false);
    }
#endif
  } else {
    ret.SetBoolKey("is_valid", false);
  }

  return ret;
}

// Converts TimeTicks to Time. The conversion will be incorrect if system
// time is adjusted between `ticks` and now.
base::Time TicksToTime(base::TimeTicks ticks) {
  base::Time now_time = base::Time::Now();
  base::TimeTicks now_ticks = base::TimeTicks::Now();
  base::TimeDelta elapsed_since_ticks = now_ticks - ticks;
  return now_time - elapsed_since_ticks;
}

}  // namespace

void ProcessNodeImplDescriber::OnPassedToGraph(Graph* graph) {
  graph->GetNodeDataDescriberRegistry()->RegisterDescriber(this,
                                                           kDescriberName);
}

void ProcessNodeImplDescriber::OnTakenFromGraph(Graph* graph) {
  graph->GetNodeDataDescriberRegistry()->UnregisterDescriber(this);
}

base::Value ProcessNodeImplDescriber::DescribeProcessNodeData(
    const ProcessNode* node) const {
  const ProcessNodeImpl* impl = ProcessNodeImpl::FromNode(node);

  base::Value ret(base::Value::Type::DICTIONARY);

  if (impl->private_footprint_kb()) {
    ret.SetIntKey("private_footprint_kb",
                  base::saturated_cast<int>(impl->private_footprint_kb()));
  }

  if (impl->resident_set_kb()) {
    ret.SetIntKey("resident_set_kb",
                  base::saturated_cast<int>(impl->resident_set_kb()));
  }

  constexpr RenderProcessHostId kInvalidRenderProcessHostId =
      RenderProcessHostId(content::ChildProcessHost::kInvalidUniqueID);
  if (impl->GetRenderProcessId() != kInvalidRenderProcessHostId) {
    ret.SetIntKey("render_process_id", impl->GetRenderProcessId().value());
  }

  // The content function returns "Tab" for renderers - whereas "Renderer" is
  // the common vernacular here.
  std::string process_type =
      content::GetProcessTypeNameInEnglish(impl->process_type());
  if (impl->process_type() == content::PROCESS_TYPE_RENDERER)
    process_type = "Renderer";
  ret.SetStringKey("process_type", process_type);

  ret.SetStringKey("pid", base::NumberToString(impl->process_id()));

  ret.SetKey("process", GetProcessValueDict(impl->process()));

  ret.SetStringKey("launch_time", base::TimeFormatTimeOfDayWithMilliseconds(
                                      TicksToTime(impl->launch_time())));

  if (impl->exit_status()) {
    ret.SetIntKey("exit_status", impl->exit_status().value());
  }

  if (!impl->metrics_name().empty()) {
    ret.SetStringKey("metrics_name", impl->metrics_name());
  }

  ret.SetBoolKey("main_thread_task_load_is_low",
                 impl->main_thread_task_load_is_low());

  ret.SetStringKey("priority", base::TaskPriorityToString(impl->priority()));

  ret.SetStringKey("hosted_content_types",
                   HostedProcessTypesToString(impl->hosted_content_types()));

  return ret;
}

}  // namespace performance_manager
