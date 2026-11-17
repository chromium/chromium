# New Tab Page Core

Host-agnostic browser-side building blocks for the New Tab Page, shared by
chrome://new-tab-page (`NewTabPageUI`) and the New Tab Page component
extension (`//chrome/browser/ui/webui/new_tab_page_extension`).

- `ntp_load_time_data.h`: data sources and `loadTimeData`.
- `new_tab_page_mojo_handlers.h`: the NTP Mojo handlers.

`:core` must not depend on an NTP host (enforced by `DEPS` for `core/` and by
`assert_no_deps` in `BUILD.gn`). One exception: `new_tab_page_handler.cc`
defines `NewTabPageUI::IsManagedProfile()` and
`NewTabPageUI::kRealboxContextualEntrypointElementId`.
