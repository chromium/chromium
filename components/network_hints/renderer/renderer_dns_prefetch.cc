// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See header file for description of RendererDnsPrefetch class

#include "components/network_hints/renderer/renderer_dns_prefetch.h"

#include <ctype.h>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/network_hints/renderer/dns_prefetch_queue.h"

namespace network_hints {
namespace {

constexpr size_t kMaxDnsHostnamesPerRequest = 30;
constexpr size_t kMaxDnsHostnameLength = 255;

}  // namespace

RendererDnsPrefetch::RendererDnsPrefetch(BatchHandler batch_handler)
    : batch_handler_(std::move(batch_handler)), c_string_queue_(1000) {
  Reset();
}

RendererDnsPrefetch::~RendererDnsPrefetch() = default;

void RendererDnsPrefetch::Reset() {
  domain_map_.clear();
  c_string_queue_.Clear();
  buffer_full_discard_count_ = 0;
  numeric_ip_discard_count_ = 0;
  new_name_count_ = 0;
}

// Push names into queue quickly!
void RendererDnsPrefetch::Resolve(const char* name, size_t length) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!length)
    return;  // Don't store empty strings in buffer.
  if (is_numeric_ip(name, length))
    return;  // Numeric IPs have no DNS lookup significance.

  size_t old_size =  c_string_queue_.Size();
  DnsQueue::PushResult result = c_string_queue_.Push(name, length);
  if (DnsQueue::SUCCESSFUL_PUSH == result) {
    if (1 == c_string_queue_.Size()) {
      DCHECK_EQ(old_size, 0u);
      if (0 != old_size)
        return;  // Overkill safety net: Don't send too many InvokeLater's.
      weak_factory_.InvalidateWeakPtrs();
      base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&RendererDnsPrefetch::SubmitHostnames,
                         weak_factory_.GetWeakPtr()),
          base::TimeDelta::FromMilliseconds(10));
    }
    return;
  }
  if (DnsQueue::OVERFLOW_PUSH == result) {
    ++buffer_full_discard_count_;
    return;
  }
  DCHECK(DnsQueue::REDUNDANT_PUSH == result);
}

// Extract data from the Queue, and then send it off the the Browser process
// to be resolved.
void RendererDnsPrefetch::SubmitHostnames() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Get all names out of the C_string_queue (into our map)
  ExtractBufferedNames();
  // TBD: IT could be that we should only extract about as many names as we are
  // going to send to the browser.  That would cause a "silly" page with a TON
  // of URLs to start to overrun the DnsQueue, which will cause the names to
  // be dropped (not stored in the queue).  By fetching ALL names, we are
  // taking on a lot of work, which may take a long time to process... perhaps
  // longer than the page may be visible!?!?!  If we implement a better
  // mechanism for doing domain_map.clear() (see end of this method), then
  // we'd automatically flush such pending work from a ridiculously link-filled
  // page.

  // Don't overload the browser DNS lookup facility, or take too long here,
  // by only sending off kMaxDnsHostnamesPerRequest names to the Browser.
  // This will help to avoid overloads when a page has a TON of links.
  std::vector<std::string> names;
  GetNamesToPrefetch(kMaxDnsHostnamesPerRequest, &names);
  if (new_name_count_ > 0 || 0 < c_string_queue_.Size()) {
    weak_factory_.InvalidateWeakPtrs();
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&RendererDnsPrefetch::SubmitHostnames,
                       weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(10));
  } else {
    // TODO(JAR): Should we only clear the map when we navigate, or reload?
    domain_map_.clear();
  }

  batch_handler_.Run(names);
}

// Pull some hostnames from the queue, and add them to our map.
void RendererDnsPrefetch::ExtractBufferedNames(size_t size_goal) {
  size_t count(0);  // Number of entries to find (0 means find all).
  if (size_goal > 0) {
    if (size_goal <= domain_map_.size())
      return;  // Size goal was met.
    count = size_goal - domain_map_.size();
  }

  std::string name;
  while (c_string_queue_.Pop(&name)) {
    DCHECK_NE(name.size(), 0u);
    // We don't put numeric IP names into buffer.
    DCHECK(!is_numeric_ip(name.c_str(), name.size()));
    DomainUseMap::iterator it;
    it = domain_map_.find(name);
    if (domain_map_.end() == it) {
      domain_map_[name] = kPending;
      ++new_name_count_;
      if (0 == count) continue;  // Until buffer is empty.
      if (1 == count) break;  // We found size_goal.
      DCHECK_GT(count, 1u);
      --count;
    } else {
      DCHECK(kPending == it->second || kLookupRequested == it->second);
    }
  }
}

void RendererDnsPrefetch::GetNamesToPrefetch(size_t max_count,
                                             std::vector<std::string>* names) {
  // We are on the renderer thread, and just need to send things to the browser.
  size_t domains_handled = 0;
  for (auto it = domain_map_.begin(); it != domain_map_.end(); ++it) {
    if (0 == (it->second & kLookupRequested)) {
      it->second |= kLookupRequested;
      domains_handled++;
      if (it->first.length() <= network_hints::kMaxDnsHostnameLength)
        names->push_back(it->first);
      if (0 == max_count) continue;  // Get all, independent of count.
      if (1 == max_count) break;
      --max_count;
      DCHECK_GE(max_count, 1u);
    }
  }
  DCHECK_GE(new_name_count_, domains_handled);
  new_name_count_ -= domains_handled;
}

// is_numeric_ip() checks to see if all characters in name are either numeric,
// or dots.  Such a name will not actually be passed to DNS, as it is an IP
// address.
bool RendererDnsPrefetch::is_numeric_ip(const char* name, size_t length) {
  // Scan for a character outside our lookup list.
  while (length-- > 0) {
    if (!isdigit(*name) && '.' != *name)
      return false;
    ++name;
  }
  return true;
}

}  // namespace predictor
