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
but if not,
they should be added here.
