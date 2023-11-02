The render pass list data are recorded using run_benchmark with top
real world sites.

The CL that hacks into display.cc to do the recording needs to be
applied to chromium:
https://chromium-review.googlesource.com/c/chromium/src/+/1962628

The command line to use on Windows to record top_real_world_desktop:
vpython3 tools\perf\run_benchmark --browser=release rendering.desktop
  --story-tag-filter=top_real_world_desktop

For each site, all frames are zipped into a single file, together with
one frame that with (nearly) full damage and maximum data size.
