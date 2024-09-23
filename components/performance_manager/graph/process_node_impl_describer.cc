// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/process_node_impl_describer.h"

#include "base/i18n/time_formatting.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/task_traits.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/public/graph/node_data_describer_registry.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "content/public/browser/child_process_host.h"

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
  content_types_vector.reserve(hosted_content_types.size());
  for (ProcessNode::ContentType content_type : hosted_content_types)
    content_types_vector.push_back(ContentTypeToString(content_type));

  std::string str = base::JoinString(content_types_vector, ", ");
  if (str.empty())
    str = "none";

  return str;
}

#if !BUILDFLAG(IS_APPLE)
const char* GetProcessPriorityString(const base::Process& process) {
  switch (process.GetPriority()) {
    case base::Process::Priority::kBestEffort:
      return "Best effort";
    case base::Process::Priority::kUserVisible:
      return "User visible";
    case base::Process::Priority::kUserBlocking:
      return "User blocking";
  }
  NOTREACHED();
}
#endif

base::Value GetProcessValueDict(const base::Process& process) {
  base::Value::Dict ret;

  // On Windows, handle is a void *. On Fuchsia it's an int. On other platforms
  // it is equal to the pid, so don't bother to record it.
#if BUILDFLAG(IS_WIN)
  ret.Set("handle",
          static_cast<int>(base::win::HandleToUint32(process.Handle())));
#elif BUILDFLAG(IS_FUCHSIA)
  ret.Set("handle", static_cast<int>(process.Handle()));
#endif

  // Most processes are not current, so only show the outliers.
  if (process.is_current()) {
    ret.Set("is_current", true);
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (process.GetPidInNamespace() != base::kNullProcessId) {
    ret.Set("pid_in_namespace", process.GetPidInNamespace());
  }
#endif

#if BUILDFLAG(IS_WIN)
  // Creation time is always available on Windows, even for dead processes.
  // On other platforms it is available only for valid processes (see below).
  ret.Set("creation_time",
          base::TimeFormatTimeOfDayWithMilliseconds(process.CreationTime()));
#endif

  if (process.IsValid()) {
    // These properties can only be accessed for valid processes.
    ret.Set("os_priority", process.GetOSPriority());
#if !BUILDFLAG(IS_APPLE)
    ret.Set("priority", GetProcessPriorityString(process));
#endif
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_WIN)
    ret.Set("creation_time",
            base::TimeFormatTimeOfDayWithMilliseconds(process.CreationTime()));
#endif
#if BUILDFLAG(IS_WIN)
    // Most processes are running, so only show the outliers.
    if (!process.IsRunning()) {
      ret.Set("is_running", false);
    }
#endif
  } else {
    ret.Set("is_valid", false);
  }

  return base::Value(std::move(ret));
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

base::Value::Dict ProcessNodeImplDescriber::DescribeProcessNodeData(
    const ProcessNode* node) const {
  const ProcessNodeImpl* impl = ProcessNodeImpl::FromNode(node);

  base::Value::Dict ret;

  ret.Set("pid", base::NumberToString(impl->GetProcessId()));

  ret.Set("process", GetProcessValueDict(impl->GetProcess()));

  ret.Set("launch_time", base::TimeFormatTimeOfDayWithMilliseconds(
                             TicksToTime(impl->GetLaunchTime())));
  ret.Set("resource_context", impl->GetResourceContext().ToString());

  if (impl->GetExitStatus()) {
    ret.Set("exit_status", impl->GetExitStatus().value());
  }

  if (!impl->GetMetricsName().empty()) {
    ret.Set("metrics_name", impl->GetMetricsName());
  }

  ret.Set("priority", base::TaskPriorityToString(impl->GetPriority()));

  if (impl->GetPrivateFootprintKb()) {
    ret.Set("private_footprint_kb",
            base::saturated_cast<int>(impl->GetPrivateFootprintKb()));
  }

  if (impl->GetResidentSetKb()) {
    ret.Set("resident_set_kb",
            base::saturated_cast<int>(impl->GetResidentSetKb()));
  }

  // The content function returns "Tab" for renderers - whereas "Renderer" is
  // the common vernacular here.
  std::string process_type =
      content::GetProcessTypeNameInEnglish(impl->GetProcessType());
  if (impl->GetProcessType() == content::PROCESS_TYPE_RENDERER) {
    process_type = "Renderer";
  }
  ret.Set("process_type", process_type);

  if (impl->GetProcessType() == content::PROCESS_TYPE_RENDERER) {
    // Renderer-only properties.
    ret.Set("render_process_id", impl->GetRenderProcessHostId().value());

    ret.Set("main_thread_task_load_is_low", impl->GetMainThreadTaskLoadIsLow());

    ret.Set("hosted_content_types",
            HostedProcessTypesToString(impl->GetHostedContentTypes()));
  } else if (impl->GetProcessType() != content::PROCESS_TYPE_BROWSER) {
    // Non-renderer child process properties.
    ret.Set("browser_child_process_host_id",
            impl->GetBrowserChildProcessHostProxy()
                .browser_child_process_host_id()
                .value());
  }

  return ret;
}

}  // namespace performance_manager
