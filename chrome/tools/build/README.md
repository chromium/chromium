'FILES.cfg' for each platform is now obsolete with respect to archiving
(crbug/1260176). New config files for archiving build artifacts are located in
src/infra/archive_config for dev builders and in
src-internal/testing/buildbot/archive for official builders.
For more information on how these files are used, refer to the proto file within
the archive module:
https://chromium.googlesource.com/chromium/tools/build.git/+/HEAD/recipes/recipe_modules/archive/properties.proto
