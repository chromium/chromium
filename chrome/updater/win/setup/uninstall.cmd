rem Deletes the script's parent directory if \AppData\Local\ChromeUpdater\ is
rem anywhere in the directory path. Sleeps 3 seconds and tries 3 times to
rem delete the directory.
@echo off
set Directory=%~dp0
@echo %Directory% | FindStr /R \\AppData\\Local\\Google\\GoogleUpdater\\ > nul
IF %ERRORLEVEL% NEQ 0 exit 1
@echo Deleting "%Directory%"...
for /L %%G IN (1,1,3) do (
  rmdir "%Directory%" /s /q > nul
  if not exist "%Directory%" exit 0
  ping -n 3 127.0.0.1 > nul
)
