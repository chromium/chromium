// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/metrics/cast_event_builder_impl.h"

#include "base/memory/ptr_util.h"
#include "base/metrics/metrics_hashes.h"
#include "net/base/ip_address.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/cast_logs.pb.h"

namespace chromecast {

namespace {

// Bitmasks and values for the |transport_connection_id| field when recording
// discovery metrics for mDNS.
const uint32_t kDiscoverySenderMask = 0x0000FFFF;
const uint32_t kDiscoveryUnicastBit = 0x80000000;

}  // namespace

//=============================================================================
// CastEventBuilderImplTest
//=============================================================================

TEST(CastEventBuilderImplTest, GetName) {
  const char kCastEvent[] = "Cast.Test.Event";
  CastEventBuilderImpl builder;
  builder.SetName(kCastEvent);
  EXPECT_STRCASEEQ(builder.GetName().c_str(), kCastEvent);
  auto proto = base::WrapUnique(builder.Build());
  EXPECT_EQ(proto->name_hash(), base::HashMetricName(kCastEvent));
}

TEST(CastEventBuilderImplTest, MergeFromCastEventProto) {
  const char kOldCastEvent[] = "Cast.Test.Event.Old";
  const char kNewCastEvent[] = "Cast.Test.Event.New";
  auto old_proto =
      base::WrapUnique(CastEventBuilderImpl().SetName(kOldCastEvent).Build());
  auto new_proto =
      base::WrapUnique(CastEventBuilderImpl().SetName(kNewCastEvent).Build());
  auto merged_proto = base::WrapUnique(CastEventBuilderImpl()
                                           .SetName(kOldCastEvent)
                                           .MergeFrom(new_proto.get())
                                           .Build());
  EXPECT_NE(old_proto->name_hash(), merged_proto->name_hash());
  EXPECT_EQ(new_proto->name_hash(), merged_proto->name_hash());
}

TEST(CastEventBuilderImplTest, DiscoveryEvent_AppSubtype) {
  const char kCastEvent[] = "Cast.Discovery.App.Subtype";

  CastEventBuilderImpl builder;
  builder.SetName(kCastEvent);
  builder.SetDiscoveryAppSubtype("1234ABCD");
  auto proto = base::WrapUnique(builder.Build());
  EXPECT_EQ(proto->name_hash(), base::HashMetricName(kCastEvent));
  EXPECT_EQ(proto->app_id(), 0x1234ABCDu);
}

TEST(CastEventBuilderImplTest, DiscoveryEvent_NamespaceSubtype) {
  const char kCastEvent[] = "Cast.Discovery.Namespace.Subtype";

  CastEventBuilderImpl builder;
  builder.SetName(kCastEvent);
  builder.SetDiscoveryNamespaceSubtype(
      "4abd03646f53722ded0335a84493136da95d5dcb");
  auto proto = base::WrapUnique(builder.Build());
  EXPECT_EQ(proto->name_hash(), base::HashMetricName(kCastEvent));
  EXPECT_EQ(proto->app_id(), 0x4ABD0364u);
}

TEST(CastEventBuilderImplTest, DiscoveryEvent_UnicastFlag) {
  const char kCastEvent[] = "Cast.Discovery.Generic.Event";

  // Set unicast bit, i.e. multicast was not used for discovery event.
  {
    CastEventBuilderImpl builder;
    builder.SetName(kCastEvent);
    builder.SetDiscoveryUnicastFlag(true);
    auto proto = base::WrapUnique(builder.Build());
    EXPECT_EQ(proto->name_hash(), base::HashMetricName(kCastEvent));
    EXPECT_TRUE(proto->transport_connection_id() & kDiscoveryUnicastBit);
  }

  // Clear unicast bit, i.e. multicast was used for discovery event.
  {
    CastEventBuilderImpl builder;
    builder.SetName(kCastEvent);
    builder.SetDiscoveryUnicastFlag(false);
    auto proto = base::WrapUnique(builder.Build());
    EXPECT_EQ(proto->name_hash(), base::HashMetricName(kCastEvent));
    EXPECT_FALSE(proto->transport_connection_id() & kDiscoveryUnicastBit);
  }
}

TEST(CastEventBuilderImplTest, DiscoveryEvent_SenderAddress) {
  const char kCastEvent[] = "Cast.Discovery.Generic.Event";

  // IPv4 sender IP address. The last 2 bytes of the address in network order
  // are grabbed and packed into 32-bit value.
  {
    CastEventBuilderImpl builder;
    builder.SetName(kCastEvent);
    net::IPAddress sender_ip;
    ASSERT_TRUE(sender_ip.AssignFromIPLiteral("172.17.36.97"));
    builder.SetDiscoverySender(sender_ip.bytes());
    auto proto = base::WrapUnique(builder.Build());
    EXPECT_EQ(proto->name_hash(), base::HashMetricName(kCastEvent));
    EXPECT_EQ(proto->transport_connection_id() & kDiscoverySenderMask,
              0x00002461u);
  }
  // IPv6 sender IP address. The last 2 bytes of the address in network order
  // are grabbed and packed into 32-bit value.
  {
    CastEventBuilderImpl builder;
    builder.SetName(kCastEvent);
    net::IPAddress sender_ip;
    ASSERT_TRUE(
        sender_ip.AssignFromIPLiteral("2620:0:1000:2100:c59c:d8ec:85fe:11fe"));
    builder.SetDiscoverySender(sender_ip.bytes());
    auto proto = base::WrapUnique(builder.Build());
    EXPECT_EQ(proto->name_hash(), base::HashMetricName(kCastEvent));
    EXPECT_EQ(proto->transport_connection_id() & kDiscoverySenderMask,
              0x000011FEu);
  }
  // IPv4 mapped address, where IPv4 address is mapped into a IPv6 address. The
  // last 2 bytes of the original IPv4 address and mapped IPv4 address should
  // match.
  // "172.17.36.97" (IPv4) -> "0:0:0:0:0:ffff:ac11:2461" (IPv6)
  {
    CastEventBuilderImpl builder;
    builder.SetName(kCastEvent);
    net::IPAddress sender_ip;
    ASSERT_TRUE(sender_ip.AssignFromIPLiteral("0:0:0:0:0:ffff:ac11:2461"));
    builder.SetDiscoverySender(sender_ip.bytes());
    auto proto = base::WrapUnique(builder.Build());
    EXPECT_EQ(proto->name_hash(), base::HashMetricName(kCastEvent));
    // Last 4 bytes of mapped IPv4 address should match original IPv4 address.
    EXPECT_EQ(proto->transport_connection_id() & kDiscoverySenderMask,
              0x00002461u);
  }
}

TEST(CastEventBuilderImplTest, DiscoveryEvent_AddressAndFlags) {
  const char kCastEvent[] = "Cast.Discovery.Generic.Event";

  CastEventBuilderImpl builder;
  builder.SetName(kCastEvent);
  builder.SetDiscoveryUnicastFlag(false);
  net::IPAddress sender_ip;
  ASSERT_TRUE(sender_ip.AssignFromIPLiteral("172.17.36.97"));
  builder.SetDiscoverySender(sender_ip.bytes());
  builder.SetDiscoveryUnicastFlag(true);
  auto proto = base::WrapUnique(builder.Build());

  EXPECT_EQ(proto->name_hash(), base::HashMetricName(kCastEvent));
  EXPECT_EQ(proto->transport_connection_id() & kDiscoverySenderMask,
            0x00002461u);
  EXPECT_TRUE(proto->transport_connection_id() & kDiscoveryUnicastBit);
}

}  // namespace chromecast
