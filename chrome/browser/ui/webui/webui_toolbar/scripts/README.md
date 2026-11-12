# Usage

Scripts to store our best set of flags.

Use this if you want to benchmark Webium top-chrome
with/without some new feature you are adding.

`run.sh` takes a binary and runs it with the correct flags.

E.g.

```
run.sh out/linux-opt/chrome --other-chrome-arguments
```

# Maintenance

As new features/optimizations are added for webium,
ideally they are on by default if webium is enabled
(by adding them as parameters on one of the webium flags)
but if not, they should be added here:

1. Edit `enabled.sh` (keeping features in lexical order).
2. Run `./generate_chrome_flags.py`, which automatically updates:
   - `chrome/browser/about_flags.cc` (`kWebUIToolbarEnableFeatures` and
     `kWebUIToolbarDisableFeatures`)
   - `tools/perf/core/bot_platforms.py` (`INITIAL_WEBUI_FEATURES`)
   - `tools/perf/core/shard_maps/*_map.json` (`linux-r350-perf`,
     `mac-m1-pro-perf`, and `win-10-perf`)
3. Run `./generate_chrome_flags.py --update-crossbench` to also update
   `config/benchmark/browser_startup/browser.config.hjson` in
   `third_party/crossbench`.
