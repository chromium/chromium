# How to Run a Fuzz Test

Create an appropriate build config:

```shell
% tools/mb/mb.py gen -m chromium.fuzz -b 'Libfuzzer Upload Linux ASan' out/libfuzzer
% gn gen out/libfuzzer
```

Build the fuzz target:

```shell
% ninja -C out/libfuzzer $TEST_NAME
```

Create an empty corpus directory if you don't have one already.

```shell
% mkdir ${TEST_NAME}_corpus
```

Turning off detection of ODR violations that occur in component builds:

```shell
% export ASAN_OPTIONS=detect_odr_violation=0
```

If the test has a seed corpus:

```shell
% ./out/libfuzzer/$TEST_NAME ${TEST_NAME}_corpus out/libfuzzer/gen/components/media_router/common/providers/cast/channel/${TEST_NAME}_corpus
```

If the test has no seed corpus, omit the last parameter:

```shell
% ./out/libfuzzer/$TEST_NAME ${TEST_NAME}_corpus
```

For more details, refer to https://chromium.googlesource.com/chromium/src/testing/libfuzzer/+/refs/heads/main/getting_started.md
