<!-- Copyright 2016 The Chromium Authors. All rights reserved.
     Use of this source code is governed by a BSD-style license that can be
     found in the LICENSE file.
-->

# How to run benchmarks for Media Router
1. Run the following command to find all the available browsers:

```shell
./chrome/test/media_router/telemetry/run_benchmark --browser list
```

Let `<browser>` be one of the results.

2. Run the following command to get benchmarks for media router dialog latency:

```shell
./chrome/test/media_router/telemetry/run_benchmark --browser=<browser> \
    media_router.cpu_memory
```

The results will be in
`<chromium src folder>/chrome/test/media_router/telemetry/results.html`
