# This component is shared with the Chrome OS build, so it's important to limit
# dependencies to a minimal set.
include_rules = [
  "-components",
  "+components/component_updater",
  "+components/flags_ui",
  "+components/keep_alive_registry",
  "+components/metrics",
  "+components/network_time",
  "+components/prefs",
  "+components/variations",
  "+components/version_info",
  "+content/public/test",
  "+crypto",
  "+extensions/buildflags",
  "+mojo/public/cpp",
  "+services/service_manager/public/cpp",
  "+services/network/test",
  "+third_party/metrics_proto",
  "+third_party/zlib/google",
  "-net",
  "+url",
]

specific_include_rules = {
  "log_decoder\.cc": [
    "+third_party/protobuf/src/google/protobuf",
  ],
}
